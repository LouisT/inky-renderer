import { html, raw } from 'hono/html';
import { getDay, epochToTime } from './_utils.mjs';

export default async function (data, mode) {
    let current = data?.currentConditions ?? {},
        forecast = (data?.days ?? []).slice(0, 7);

    return html`
<html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Retro Weather</title>
        <link rel="stylesheet" href="/styles/retro.css">
        <link rel="stylesheet" href="/styles/queries.css">
        <link rel="stylesheet" href="/styles/weather.css">
    </head>
    <body>
        <a href="/" class="home-btn">Home</a>
        <div class="container">
            <div class="content sp:bg-2 sl:bg-2">
                <div class="sp:color-5 sl:color-5" style="display:flex; flex-direction:column; align-items:center; margin-bottom:0.5rem; margin-top:1.2rem;">
                    <i class="w-${current.icon ?? 'clear-day'} sp:fs-4xl sl:fs-4xl lp:fs-8xl ll:fs-8xl"></i>
                    <p style="margin:0;margin-top:0.5rem;text-align:center;">
                        <strong>${current.temp ?? '???'}°F (${current.feelslike ?? '???'}°F)</strong><br>
                        ${current.conditions ?? '???'} at ${epochToTime(data?.currentConditions?.datetimeEpoch, data?.timezone)}
                    </p>
                </div>
                <hr>
                <span class="center">
                    <table>
                        <thead>
                            <tr>
                                <th>Day</th>
                                <th>High</th>
                                <th>Low</th>
                                <th class="for-600-448 for-1200-825 for-825-1200">Conditions</th>
                                <th></th>
                            </tr>
                        </thead>
                        <tbody>
                            ${forecast.map((day, idx) => html`
                            <tr class="${idx >= 3 ? 'for-448-600 for-1200-825 for-825-1200' : ''}">
                                <td>${getDay(day.datetime) ?? '???'} </td>
                                <td>${day.tempmax ?? '???'}°F</td>
                                <td>${day.tempmin ?? '???'}°F</td>
                                <td class="for-600-448 for-1200-825 for-825-1200">${day.conditions ?? '???'} </td>
                                <td><i class="w-${day.icon ?? 'clear-day'} fs-2xl"></i></td>
                            </tr>
                        `)}
                        </tbody>
                    </table>
                </span>
            </div>
            <footer class="sp:fs-xxs">
                ${data?.address ?? '???'}
            </footer>
        </div>
    </body>
</html>
    `;
};