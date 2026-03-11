const express = require('express');
const cors = require('cors');
const path = require('path');
const { GoogleGenerativeAI } = require('@google/generative-ai');

const app = express();
const port = 3000;

app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, "public")));

const apikey = 'AIzaSyDEpaHnDUPqGWDnT8K_1KcZ8YZumswPpRY';
const genAI = new GoogleGenerativeAI(apikey);
const model = genAI.getGenerativeModel({ model: "gemini-2.0-flash" });


let latestData = {
  heartRate: 0,
  spo2: 0,
  temperature: 0,
  humidity: 0,
  pressure: 0,
  steps: 0,
  hydrationScore: 0,
  hydrationAdvice: "",
  isWorn: false,
  isMoving: false,
  time: "",
};

let lastAIResponse = "";

// ============================
//   RECEIVE DATA FROM ESP8266
// ============================
app.post("/data", (req, res) => {
  const data = req.body || {};
  const prevSteps = latestData.steps;

  const numericFields = [
    "heartRate",
    "spo2",
    "temperature",
    "humidity",
    "pressure",
    "steps",
    "hydrationScore"
  ];

  numericFields.forEach(field => {
    if (data[field] !== undefined) {
      latestData[field] = Number(data[field]);
    }
  });

  latestData.hydrationAdvice = data.hydrationAdvice || "No advice";
  latestData.time = data.time || "";

  // Worn detection (direct boolean)
  latestData.isWorn = Boolean(data.obstacle);

  // Movement detection (corrected)
  latestData.isMoving = latestData.steps > prevSteps;

  console.log("Updated Sensor Data:", latestData);
  res.sendStatus(200);
});

// ============================
//        GET LATEST DATA
// ============================
app.get("/data", (req, res) => {
  res.json(latestData);
});

// ============================
//      AI ANALYSIS ENDPOINT
// ============================
app.post("/ask-ai", async (req, res) => {
  const query = req.body?.query || "";
  if (!query.trim()) return res.status(400).send({ error: "Empty query." });

  const prompt = `
You are a health AI assistant. Here is the user's latest health data:

Heart Rate: ${latestData.heartRate} bpm
SpO₂: ${latestData.spo2}%
Steps: ${latestData.steps}
Temp: ${latestData.temperature}°C
Humidity: ${latestData.humidity}%
Pressure: ${latestData.pressure} hPa

Hydration Score: ${latestData.hydrationScore}
Hydration Advice: ${latestData.hydrationAdvice}

Watch Worn: ${latestData.isWorn ? "YES" : "NO"}
Motion: ${latestData.isMoving ? "Active" : "At Rest"}
Time: ${latestData.time}

User asks: "${query}"
Provide a clear, medically safe explanation.
`;

  try {
    const result = await model.generateContent(prompt);
    const text = result?.response?.text?.() || "No response";

    lastAIResponse = text;
    console.log("AI Response:", text);

    res.send({ response: text });
  } catch (err) {
    console.error("Gemini API Error:", err);
    res.status(500).send({ error: "AI request failed" });
  }
});

// ============================
//       LAST AI RESPONSE
// ============================
app.get("/last-ai", (req, res) => {
  res.json({ response: lastAIResponse });
});

// ============================
//       INDEX ROUTE
// ============================
app.get("/", (req, res) => {
  res.sendFile(path.join(__dirname, "public/index.html"));
});

// ============================
app.listen(port, "0.0.0.0", () => {
  console.log(`Server running at http://localhost:${port}`);
});
