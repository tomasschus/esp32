export interface GraphHopperHit {
  name: string;
  point: { lat: number; lng: number };
  extent?: [number, number, number, number];
  country?: string;
  countrycode?: string;
  city?: string;
  state?: string;
  street?: string;
  postcode?: string;
  housenumber?: string;
  osm_id?: number;
  osm_type?: string;
}

export interface GraphHopperGeocodeResponse {
  hits: GraphHopperHit[];
  locale: string;
}

export interface GeocodedHit {
  label: string;
  lat: number;
  lon: number;
  street?: string;
  housenumber?: string;
  city?: string;
  state?: string;
  postcode?: string;
  country?: string;
  countrycode?: string;
  extent?: [number, number, number, number];
  osm_id?: number;
  osm_type?: string;
}
