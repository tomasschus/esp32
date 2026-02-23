import { Controller, Post, Body, BadRequestException } from '@nestjs/common';
import { RouteService } from './route.service';
import type { RouteRequest } from '../types/route';

@Controller('route')
export class RouteController {
  constructor(private readonly routeService: RouteService) {}

  @Post()
  async route(@Body() body: RouteRequest) {
    if (!body.from || !body.to) {
      throw new BadRequestException(
        'Los parámetros "from" y "to" son requeridos',
      );
    }
    return this.routeService.getRoute(body);
  }
}
