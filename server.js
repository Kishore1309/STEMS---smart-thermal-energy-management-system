// ================= VARIABLES =================
let latestCommand = "OFF";
let history = [];
let fullHistory = [];



// ================= IMPORTS =================
const express = require("express");
const app = express();
const http = require("http").createServer(app);
const io = require("socket.io")(http);

// ================= SERVE FRONTEND =================
app.use(express.static("public"));

// ================= RECEIVE ESP32 DATA =================
app.get("/data", (req, res) => {
  const t1 = Number(req.query.t1);
  const t2 = Number(req.query.t2);
  const t3 = Number(req.query.t3);
  const current = Number(req.query.current);
  const voltage = Number(req.query.voltage);
  let battery = ((voltage - 9.0) / (12.6 - 9.0)) * 100;
  battery = Math.max(0, Math.min(100, battery)); // Clamp between 0% and 100%

  const data = { t1, t2, t3, current, voltage, battery };

  console.log("📡 DATA:", data);

  // ================= FULL HISTORY LOGIC =================
  const avgTemp = (t1 + t2 + t3) / 3;
  const timestamp = new Date().toISOString();
  const newRecord = { timestamp, t1, t2, t3, current, voltage, battery, avgTemp };

  fullHistory.push(newRecord);
  if (fullHistory.length > 200) fullHistory.shift();

  // ================= ML LOGIC =================
  history.push(newRecord);
  if (history.length > 20) history.shift();

  let predicted = avgTemp;

  if (history.length >= 2) {
    let slope = history[history.length - 1].t1 - history[history.length - 2].t1;
    predicted = t1 + slope * 3;
  }

  console.log("🔥 Predicted Temp:", predicted);

  // ================= DECISION =================
  if (predicted > 36) latestCommand = "PELTIER";
  else if (predicted > 33) latestCommand = "FAN_HIGH";
  else if (predicted > 30) latestCommand = "FAN_LOW";
  else latestCommand = "OFF";

  let risk = "SAFE";
  if (predicted > 45) risk = "CRITICAL";
  else if (predicted > 35) risk = "WARNING";

  newRecord.predicted = predicted;
  newRecord.risk = risk;  
  newRecord.command = latestCommand;

  console.log("❄️ Command:", latestCommand);

  // ================= SEND TO UI =================
  // add prediction + command to data
  data.predicted = predicted;
  data.command = latestCommand;

  // send to UI
  io.emit("data", data);

  res.send("OK");
});

// ================= SEND COMMAND TO ESP32 =================
app.get("/command", (req, res) => {
  res.send(latestCommand);
});

// ================= API HISTORY =================
app.get("/api/history", (req, res) => {
  res.json(fullHistory);
});

// ================= SOCKET CONNECTION (OPTIONAL LOG) =================
io.on("connection", () => {
  console.log("🟢 Web client connected");
});

// ================= START SERVER =================
http.listen(3000, "0.0.0.0", () => {
  console.log("🚀 Server running");
  console.log("👉 http://localhost:3000");
});