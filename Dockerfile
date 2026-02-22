FROM node:20-alpine
ENV NODE_ENV=development
WORKDIR /app
COPY backend/package*.json ./
RUN npm install
COPY backend/tsconfig.json ./
COPY backend/src ./src
RUN node_modules/.bin/tsc && ls /app/dist
RUN npm prune --production && rm -rf tsconfig.json src
ENV NODE_ENV=production
EXPOSE 4500
CMD ["node", "dist/server.js"]
