import express from "express";
import geocodeRouter from "./routes/geocode";
import routeRouter from "./routes/route";

const app = express();
const PORT = process.env.PORT ?? 4500;

app.use(express.json());

app.use("/geocode", geocodeRouter);
app.use("/route", routeRouter);

app.listen(PORT, () => {
  console.log(`Backend corriendo en http://localhost:${PORT}`);
});
