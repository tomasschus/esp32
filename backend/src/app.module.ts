import { Module } from '@nestjs/common';
import { AppController } from './app.controller';
import { GeocodeModule } from './geocode/geocode.module';
import { RouteModule } from './route/route.module';

@Module({
  imports: [GeocodeModule, RouteModule],
  controllers: [AppController],
})
export class AppModule {}
