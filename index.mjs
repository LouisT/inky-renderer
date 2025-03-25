import pkg from './package.json' assert { type: 'json' };
import { Hono } from 'hono';
import { basicAuth } from 'hono/basic-auth';
import {
    v0, v1,
} from './routes/index.mjs';

// Create the app
const app = new Hono();

// Redirect to the github repo
app.get('/', (c) => c.redirect(pkg.repository));

// Set up routes
app.route('/', v0);
app.route('/', v1);

// Send anything not matching to 404
app.get("/*", async (c) => c.json({ error: "Not Found" }, 404));

// Export the app
export default app