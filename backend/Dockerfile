FROM node:20-alpine AS builder
WORKDIR /app
COPY package*.json ./
RUN npm ci
COPY tsconfig.json ./
COPY src ./src
RUN npm run build
# DEBUG: mostrar contenido de /app y /app/dist para diagnóstico en Coolify
RUN echo "DEBUG: listing /app" && ls -la /app || true
RUN echo "DEBUG: listing /app/dist" && ls -la /app/dist || true

FROM node:20-alpine
WORKDIR /app
COPY package*.json ./
RUN npm ci --omit=dev
COPY --from=builder /app/dist ./dist
EXPOSE 4500
CMD ["node", "dist/server.js"]
