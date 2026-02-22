FROM node:20-alpine AS base

WORKDIR /app

# Dependencias
FROM base AS deps
COPY backend/package*.json ./
RUN npm install

# Build
FROM base AS builder
COPY --from=deps /app/node_modules ./node_modules
COPY backend/. .
RUN npm run build \
    && npm prune --production

# Producción
FROM base AS runner
ENV NODE_ENV=production
COPY --from=builder /app/package.json ./
COPY --from=builder /app/node_modules ./node_modules
COPY --from=builder /app/dist ./dist

EXPOSE 4500
CMD ["sh", "-c", "node dist/server.js"]
