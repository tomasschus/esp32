export interface RouteRequest {
  from: [number, number]; // [lon, lat]
  to: [number, number]; // [lon, lat]
  profile?: string;
}

export interface GraphHopperInstruction {
  distance: number;
  time: number;
  sign: number;
  text: string;
  street_name: string;
  interval: [number, number];
  heading?: number;
  last_heading?: number;
}

export interface GraphHopperPath {
  distance: number;
  time: number;
  points: string;
  points_encoded: boolean;
  points_encoded_multiplier: number;
  bbox: [number, number, number, number];
  instructions: GraphHopperInstruction[];
  ascend: number;
  descend: number;
  snapped_waypoints: string;
  details: Record<string, [number, number, unknown][]>;
}

export interface GraphHopperRouteResponse {
  paths: GraphHopperPath[];
  info: { copyrights: string[]; took: number };
}

export interface RouteResponse {
  distance: number;
  time: number;
  bbox: [number, number, number, number];
  points: string;
  points_encoded_multiplier: number;
  instructions: Pick<
    GraphHopperInstruction,
    'text' | 'street_name' | 'distance' | 'time' | 'sign' | 'interval'
  >[];
  details: GraphHopperPath['details'];
  ascend: number;
  descend: number;
  snapped_waypoints: string;
}
