import { Injectable, HttpException } from '@nestjs/common';
import type {
  GraphHopperGeocodeResponse,
  GeocodedHit,
  GraphHopperHit,
} from '../types/geocode';

const GRAPHHOPPER_API_KEY =
  process.env.GRAPHHOPPER_API_KEY ?? '0086de71-3a18-474a-a401-139651689d1f';

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

    const hits: GeocodedHit[] = data.hits.map((hit) => ({
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

    return { hits };
  }
}
