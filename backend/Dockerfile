FROM node:20-alpine
WORKDIR /app
COPY package*.json ./
RUN npm ci
COPY tsconfig.json ./
COPY src ./src
RUN npm run build && ls /app/dist
RUN npm prune --production && rm -rf src tsconfig.json
EXPOSE 4500
CMD ["node", "dist/server.js"]
