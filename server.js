// ================= VARIABLES =================
let latestCommand = "OFF";
let history = [];

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
  const battery = Number(req.query.battery);

  const data = { t1, t2, t3, current, voltage, battery };

  console.log("📡 DATA:", data);

  // ================= ML LOGIC =================
  history.push(t1);
  if (history.length > 5) history.shift();

  let predicted = t1;

  if (history.length >= 2) {
    let slope = history[history.length - 1] - history[history.length - 2];
    predicted = t1 + slope * 3;
  }

  console.log("🔥 Predicted Temp:", predicted);

  // ================= DECISION =================
  if (predicted > 50) latestCommand = "PELTIER";
  else if (predicted > 45) latestCommand = "FAN_HIGH";
  else if (predicted > 35) latestCommand = "FAN_LOW";
  else latestCommand = "OFF";

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

// ================= SOCKET CONNECTION (OPTIONAL LOG) =================
io.on("connection", () => {
  console.log("🟢 Web client connected");
});

// ================= START SERVER =================
http.listen(3000, "0.0.0.0", () => {
  console.log("🚀 Server running");
  console.log("👉 http://localhost:3000");
});