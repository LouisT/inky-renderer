#ifndef OTA_HTML_H
#define OTA_HTML_H

// HTML for the OTA update page
// Compressed and ugly to make it smaller
const char *ota_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>OTA</title>
<style>
body{background:#121212;color:#e0e0e0;font-family:monospace;display:flex;flex-direction:column;align-items:center;justify-content:center;height:75vh;margin:0}
.c{background:#1e1e1e;padding:2rem;border-radius:12px;width:300px;text-align:center}
.b{width:100%;padding:12px;margin-top:15px;background:#03dac6;border:0;border-radius:6px;cursor:pointer;font-weight:bold}
select,input{width:100%;margin-bottom:10px;background:#333;color:#fff;border:1px solid #444;padding:8px;box-sizing:border-box}
.p{width:100%;background:#333;height:20px;margin-top:15px;display:none;border-radius:4px}
.f{width:0;height:100%;background:#bb86fc;transition:width .2s;border-radius:4px}
</style>
</head>
<body>
<h1>Inky Renderer OTA</h1>
<div class="c">
<select id="t"><option value="fw">Firmware</option><option value="fs">LittleFS</option></select>
<input type="file" id="f">
<button class="b" onclick="u()">Upload & Flash</button>
<div class="p" id="p"><div class="f" id="b"></div></div>
<p id="s"></p>
</div>
<script>
function u(){
var f=document.getElementById('f').files[0],x=new XMLHttpRequest(),s=document.getElementById('s');
if(!f){s.innerText='No file selected';return}
x.upload.onprogress=(e)=>{document.getElementById('p').style.display='block';document.getElementById('b').style.width=Math.round(e.loaded/e.total*100)+'%'};
x.onload=()=>{s.innerText=x.status==200?'Success! Rebooting...':'Error: '+x.status};
x.onerror=()=>{s.innerText='Upload Failed'};
var d=new FormData();d.append('update',f);
x.open('POST','/update?type='+document.getElementById('t').value);x.send(d)}
</script>
</body>
</html>
)rawliteral";

#endif