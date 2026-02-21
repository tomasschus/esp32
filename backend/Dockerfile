FROM node:20-alpine
WORKDIR /app
COPY package*.json ./
RUN npm ci
COPY tsconfig.json ./
COPY src ./src
RUN echo "=== tsc version ===" \
    && node_modules/.bin/tsc --version \
    && echo "=== running tsc ===" \
    && node_modules/.bin/tsc 2>&1 \
    && echo "=== dist contents ===" \
    && ls /app/dist
RUN npm prune --production && rm -rf src tsconfig.json
EXPOSE 4500
CMD ["node", "dist/server.js"]
