import { Router, Request, Response } from "express";
import type {
  RouteRequest,
  GraphHopperRouteResponse,
  RouteResponse,
} from "../types/route";

const GRAPHHOPPER_API_KEY =
  process.env.GRAPHHOPPER_API_KEY ?? "0086de71-3a18-474a-a401-139651689d1f";

const ROUTE_DETAILS = [
  "road_class",
  "road_environment",
  "road_access",
  "surface",
  "max_speed",
  "average_speed",
  "toll",
  "track_type",
  "country",
];

const router = Router();

// POST /route
// body: { from: [lon, lat], to: [lon, lat], profile?: string }
router.post("/", async (req: Request, res: Response) => {
  const { from, to, profile = "car" } = req.body as RouteRequest;

  if (!from || !to) {
    res.status(400).json({ error: 'Los parámetros "from" y "to" son requeridos' });
    return;
  }

  try {
    const response = await fetch(
      `https://graphhopper.com/api/1/route?key=${GRAPHHOPPER_API_KEY}`,
      {
        method: "POST",
        headers: { "content-type": "application/json", accept: "application/json" },
        body: JSON.stringify({
          points: [from, to],
          profile,
          elevation: true,
          instructions: true,
          locale: "es",
          points_encoded: true,
          points_encoded_multiplier: 1000000,
          details: ROUTE_DETAILS,
        }),
      }
    );

    if (!response.ok) {
      res.status(response.status).json({ error: await response.text() });
      return;
    }

    const data: GraphHopperRouteResponse = await response.json();
    const path = data.paths[0];

    const result: RouteResponse = {
      distance: path.distance,
      time: path.time,
      bbox: path.bbox,
      points: path.points,
      points_encoded_multiplier: path.points_encoded_multiplier,
      instructions: path.instructions.map((i) => ({
        text: i.text,
        street_name: i.street_name,
        distance: i.distance,
        time: i.time,
        sign: i.sign,
        interval: i.interval,
      })),
      details: path.details,
      ascend: path.ascend,
      descend: path.descend,
      snapped_waypoints: path.snapped_waypoints,
    };

    res.json(result);
  } catch (err) {
    console.error("Error al consultar GraphHopper route:", err);
    res.status(500).json({ error: "Error interno del servidor" });
  }
});

export default router;
