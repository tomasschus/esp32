FROM node:20-alpine
WORKDIR /app
COPY package*.json ./
RUN npm install
COPY tsconfig.json ./
COPY src ./src
RUN node_modules/.bin/tsc && ls /app/dist
RUN npm prune --production && rm -rf src tsconfig.json
EXPOSE 4500
CMD ["node", "dist/server.js"]
