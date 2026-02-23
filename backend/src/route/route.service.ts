import { Injectable, HttpException } from '@nestjs/common';
import type {
  RouteRequest,
  GraphHopperRouteResponse,
  RouteResponse,
} from '../types/route';

const GRAPHHOPPER_API_KEY =
  process.env.GRAPHHOPPER_API_KEY ?? '0086de71-3a18-474a-a401-139651689d1f';

const ROUTE_DETAILS = [
  'road_class',
  'road_environment',
  'road_access',
  'surface',
  'max_speed',
  'average_speed',
  'toll',
  'track_type',
  'country',
];

@Injectable()
export class RouteService {
  async getRoute({
    from,
    to,
    profile = 'car',
  }: RouteRequest): Promise<RouteResponse> {
    const response = await fetch(
      `https://graphhopper.com/api/1/route?key=${GRAPHHOPPER_API_KEY}`,
      {
        method: 'POST',
        headers: {
          'content-type': 'application/json',
          accept: 'application/json',
        },
        body: JSON.stringify({
          points: [from, to],
          profile,
          elevation: true,
          instructions: true,
          locale: 'es',
          points_encoded: true,
          points_encoded_multiplier: 1000000,
          details: ROUTE_DETAILS,
        }),
      },
    );

    if (!response.ok) {
      throw new HttpException(await response.text(), response.status);
    }

    const data: GraphHopperRouteResponse = await response.json();
    const path = data.paths[0];

    return {
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
  }
}
