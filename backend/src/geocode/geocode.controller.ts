import { Controller, Get, Query, BadRequestException } from '@nestjs/common';
import { GeocodeService } from './geocode.service';

@Controller('geocode')
export class GeocodeController {
  constructor(private readonly geocodeService: GeocodeService) {}

  @Get()
  async geocode(
    @Query('q') q: string,
    @Query('lat') lat?: string,
    @Query('lon') lon?: string,
  ) {
    if (!q) {
      throw new BadRequestException('El parámetro "q" es requerido');
    }
    return this.geocodeService.geocode(q, lat, lon);
  }
}
