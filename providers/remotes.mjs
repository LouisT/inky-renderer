import articles from "./templates/articles.mjs";

const providers = {
    "google-calendar": {
        description: "public Google Calendar",
        link: async (data, mode, c) => {
            let calendarId = c.req.query('calendarId') || c.env.DEFAULT_GOOGLE_CALENDAR_ID || '',
                _mode = c.req.query('mode') || 'MONTH',
                ctz = c.req.param('timezone')
                    ?? c.req.query('timezone')
                    ?? c.req.query('zone')
                    ?? c.req.query('tz')
                    ?? c.req.header('x-timezone')
                    ?? c.req.header('x-tz')
                    ?? c.req?.raw?.cf?.timezone
                    ?? 'America/Los_Angeles';

            // Convert calendarId to an array of src's split by comma
            // Add an array of colors for each calendar
            let colors = ['%23b90e28', '%230b8043', '%2328754e', '%23711616', '%230d7813', '%23125a12', '%232a4b8d', '%23737317', '%23125a12', '%230b8043'];
            calendarId = calendarId.split(',').map((id, index) => `${encodeURIComponent(id)}&color=${colors[index % colors.length]}`).join('&src=');

            // Return the website url
            return new URL(`https://calendar.google.com/calendar/u/0/embed?height=${mode.h}&wkst=1&ctz=${ctz}&showPrint=0&showTabs=0&showCalendars=0&showNav=0&showTitle=0&mode=${_mode}&src=${calendarId}`);
        },
        // Selector for the target element
        target: 'body'
    }
}

export {
    providers as default,
    providers
}