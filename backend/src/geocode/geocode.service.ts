import { HttpException, Injectable } from '@nestjs/common';
import type {
  GeocodedHit,
  GraphHopperGeocodeResponse,
  GraphHopperHit,
} from '../types/geocode';

const GRAPHHOPPER_API_KEY =
  process.env.GRAPHHOPPER_API_KEY ?? '0086de71-3a18-474a-a401-139651689d1f';

/** Distancia aproximada en metros (Haversine). Usado solo para ordenar por cercanía. */
function distanceMeters(
  lat1: number,
  lon1: number,
  lat2: number,
  lon2: number,
): number {
  const R = 6371000; // radio Tierra en m
  const dLat = ((lat2 - lat1) * Math.PI) / 180;
  const dLon = ((lon2 - lon1) * Math.PI) / 180;
  const a =
    Math.sin(dLat / 2) ** 2 +
    Math.cos((lat1 * Math.PI) / 180) *
    Math.cos((lat2 * Math.PI) / 180) *
    Math.sin(dLon / 2) ** 2;
  const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
  return R * c;
}

function buildLabel(hit: GraphHopperHit): string {
  const parts: string[] = [];
  if (hit.street) {
    parts.push(
      hit.housenumber ? `${hit.street} ${hit.housenumber}` : hit.street,
    );
  } else {
    parts.push(hit.name);
  }
  if (hit.city) parts.push(hit.city);
  if (hit.state) parts.push(hit.state);
  if (hit.country) parts.push(hit.country);
  return parts.join(', ');
}

@Injectable()
export class GeocodeService {
  async geocode(
    q: string,
    lat?: string,
    lon?: string,
  ): Promise<{ hits: GeocodedHit[] }> {
    const params = new URLSearchParams({
      key: GRAPHHOPPER_API_KEY,
      q,
      provider: 'default',
      locale: 'es',
      location_bias_scale: '0.5',
      zoom: '9',
    });
    for (const tag of ['!place:county', '!boundary', '!historic']) {
      params.append('osm_tag', tag);
    }
    if (lat && lon) {
      params.append('point', `${lat},${lon}`);
    }

    const response = await fetch(
      `https://graphhopper.com/api/1/geocode?${params.toString()}`,
      { headers: { accept: 'application/json' } },
    );

    if (!response.ok) {
      throw new HttpException(await response.text(), response.status);
    }

    const data: GraphHopperGeocodeResponse = await response.json();

    let hits: GeocodedHit[] = data.hits.map((hit) => ({
      label: buildLabel(hit),
      lat: hit.point.lat,
      lon: hit.point.lng,
      street: hit.street,
      housenumber: hit.housenumber,
      city: hit.city,
      state: hit.state,
      postcode: hit.postcode,
      country: hit.country,
      countrycode: hit.countrycode,
      extent: hit.extent,
      osm_id: hit.osm_id,
      osm_type: hit.osm_type,
    }));

    // Priorizar por cercanía a la ubicación del usuario (estilo Google Maps).
    if (lat != null && lon != null) {
      const userLat = parseFloat(lat);
      const userLon = parseFloat(lon);
      if (!Number.isNaN(userLat) && !Number.isNaN(userLon)) {
        hits = [...hits].sort(
          (a, b) =>
            distanceMeters(userLat, userLon, a.lat, a.lon) -
            distanceMeters(userLat, userLon, b.lat, b.lon),
        );
      }
    }

    return { hits };
  }
}
