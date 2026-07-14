/* Custom JS for Flipper File Manager
 * Drop into apps_data/lan_tester/web/custom.js on SD card.
 */
(function() {
"use strict";

var t = document.querySelector("table");
if (!t) return;

var token = (location.search.match(/[?&]t=([^&]+)/) || [])[1] || "";
var tsuf = token ? "?t=" + token : "";
var cs = getComputedStyle(document.body);
var bg = cs.backgroundColor, fg = cs.color;

/* ===== Restructure layout ===== */

/* Grab existing elements */
var h1 = document.querySelector("h1");
var pathDiv = document.querySelector(".p");
/* Find "Up" button by text content, not just class (DL buttons share the same classes) */
var upLink = null;
var allLinks = document.querySelectorAll("a.b.bs");
for (var li = 0; li < allLinks.length; li++) {
    if (allLinks[li].textContent.trim() === "Up") { upLink = allLinks[li]; break; }
}
var uploadDiv = document.querySelector(".uf");
var mkdirDiv = document.querySelector(".mf");
var footer = document.querySelector(".ft");

/* --- Header row: title left, upload right --- */
var header = document.createElement("div");
header.style.cssText = "display:flex;justify-content:space-between;align-items:flex-start;" +
    "gap:8px;margin-bottom:4px;flex-wrap:wrap";

var titleArea = document.createElement("div");
if (h1) { h1.parentNode.removeChild(h1); titleArea.appendChild(h1); }

/* Breadcrumbs */
if (pathDiv) {
    pathDiv.parentNode.removeChild(pathDiv);
    var path = pathDiv.textContent.trim();
    var parts = path.split("/").filter(Boolean);
    var bhtml = "<a href='/browse/" + tsuf + "' style='color:inherit'>/</a>";
    var acc = "";
    parts.forEach(function(p) {
        acc += "/" + p;
        bhtml += " <a href='/browse" + acc + tsuf + "' style='color:inherit'>" + p + "</a> /";
    });
    pathDiv.innerHTML = bhtml;
    pathDiv.style.marginBottom = "0";
    titleArea.appendChild(pathDiv);
}

/* Compact upload */
if (uploadDiv) {
    uploadDiv.parentNode.removeChild(uploadDiv);
    uploadDiv.style.cssText = "margin:0;padding:6px 10px;border:1px solid;opacity:0.8;" +
        "display:flex;align-items:center;gap:6px;flex-shrink:0";
    /* Style the file input smaller */
    var fileInput = uploadDiv.querySelector("input[type='file']");
    if (fileInput) fileInput.style.cssText = "max-width:140px;font-size:12px";
}

header.appendChild(titleArea);
if (uploadDiv) header.appendChild(uploadDiv);
document.body.insertBefore(header, document.body.firstChild);

/* Remove mkdir from its original place */
if (mkdirDiv) mkdirDiv.parentNode.removeChild(mkdirDiv);

/* --- Toolbar: [Up] [Del sel.] ... [New folder][Create] [Filter] --- */
var toolbar = document.createElement("div");
toolbar.style.cssText = "display:flex;gap:6px;align-items:center;margin-bottom:6px;flex-wrap:wrap";

/* Left group: Up + Del sel. */
if (upLink) {
    upLink.parentNode.removeChild(upLink);
    upLink.style.cssText += ";flex-shrink:0;padding:4px 10px;font-size:13px";
    toolbar.appendChild(upLink);
} else {
    /* Root directory — show disabled Up */
    var disabledUp = document.createElement("span");
    disabledUp.textContent = "Up";
    disabledUp.className = "b bs";
    disabledUp.style.cssText = "flex-shrink:0;padding:4px 10px;font-size:13px;opacity:0.4;cursor:default";
    toolbar.appendChild(disabledUp);
}

var batchBtn = document.createElement("button");
batchBtn.textContent = "Del sel.";
batchBtn.className = "b bs";
batchBtn.style.cssText += ";padding:4px 8px;font-size:13px;flex-shrink:0";
toolbar.appendChild(batchBtn);

/* Spacer pushes right group to the end */
var spacer = document.createElement("div");
spacer.style.flex = "1";
toolbar.appendChild(spacer);

/* Right group: New folder + Filter */
if (mkdirDiv) {
    var mkdirForm = mkdirDiv.querySelector("form");
    if (mkdirForm) {
        mkdirForm.style.cssText = "display:flex;gap:4px;align-items:center;margin:0;flex-shrink:0";
        var mkdirInput = mkdirForm.querySelector("input[type='text']");
        if (mkdirInput) {
            mkdirInput.style.cssText = "width:100px;padding:4px 6px;background:" + bg +
                ";color:" + fg + ";border:1px solid;font-family:inherit;font-size:13px";
            mkdirInput.placeholder = "New folder";
            mkdirInput.removeAttribute("size");
        }
        var mkdirBtn = mkdirForm.querySelector("button");
        if (mkdirBtn) mkdirBtn.style.cssText += ";padding:4px 8px;font-size:13px";
        toolbar.appendChild(mkdirForm);
    }
}

var searchBox = document.createElement("input");
searchBox.type = "text";
searchBox.placeholder = "Filter...";
searchBox.style.cssText = "width:120px;padding:4px 8px;background:" + bg +
    ";color:" + fg + ";border:1px solid;opacity:0.8;font-family:inherit;font-size:13px;flex-shrink:0";
toolbar.appendChild(searchBox);

t.parentNode.insertBefore(toolbar, t);

/* ===== Drag & drop on upload area ===== */
var uploadForm = uploadDiv ? uploadDiv.querySelector("form") : null;
if (uploadDiv && uploadForm) {
    var origHTML = uploadDiv.innerHTML;
    uploadDiv.style.transition = "opacity 0.2s";
    uploadDiv.addEventListener("dragover", function(e) {
        e.preventDefault();
        uploadDiv.style.opacity = "0.5";
    });
    uploadDiv.addEventListener("dragleave", function() {
        uploadDiv.style.opacity = "0.8";
    });
    uploadDiv.addEventListener("drop", function(e) {
        e.preventDefault();
        uploadDiv.style.opacity = "0.8";
        var files = e.dataTransfer.files;
        if (!files.length) return;
        var fd = new FormData();
        fd.append("file", files[0]);
        var x = new XMLHttpRequest();
        x.open("POST", uploadForm.action);
        uploadDiv.innerHTML = "<span>Uploading...</span>";
        x.onload = function() { location.reload(); };
        x.onerror = function() { uploadDiv.innerHTML = origHTML; alert("Upload failed"); };
        x.send(fd);
    });
}

/* ===== Table: add thead + checkbox column ===== */
var headerRow = t.querySelector("tr");
var theadEl = document.createElement("thead");
theadEl.appendChild(headerRow);
t.insertBefore(theadEl, t.firstChild);

/* Checkbox header */
var thCheck = document.createElement("th");
thCheck.style.cssText = "width:20px;padding:6px 2px";
thCheck.innerHTML = "<input type='checkbox' id='selAll'>";
headerRow.insertBefore(thCheck, headerRow.firstChild);

/* Checkbox per row */
var allRows = [].slice.call(t.tBodies[0].rows);
allRows.forEach(function(r) {
    var td = document.createElement("td");
    td.style.cssText = "width:20px;padding:5px 2px;text-align:center";
    td.innerHTML = "<input type='checkbox' class='sel'>";
    r.insertBefore(td, r.firstChild);
});

document.getElementById("selAll").onchange = function() {
    var c = this.checked;
    [].slice.call(document.querySelectorAll(".sel")).forEach(function(cb) {
        if (cb.closest("tr").style.display !== "none") cb.checked = c;
    });
};

/* ===== Sort ===== */
var headers = t.querySelectorAll("th");
/* 0=chk, 1=Name, 2=Size, 3=Actions */
var sortCol = 1, sortAsc = true;

function rows() { return [].slice.call(t.tBodies[0].rows); }
function cell(r, c) { return (r.cells[c] || {textContent:""}).textContent.trim().toLowerCase(); }
function isDir(r) { return !!r.querySelector(".d"); }

function parseSize(s) {
    s = s.trim();
    if (s === "-") return -1;
    var m = s.match(/([\d.]+)\s*(B|KB|MB|GB)?/i);
    if (!m) return 0;
    var n = parseFloat(m[1]), u = (m[2]||"B").toUpperCase();
    if (u==="KB") n*=1024; else if (u==="MB") n*=1048576; else if (u==="GB") n*=1073741824;
    return n;
}

function doSort() {
    var r = rows(), d = r.filter(isDir), f = r.filter(function(x){return !isDir(x);});
    var cmp = sortCol === 2
        ? function(a,b){ var v=parseSize(cell(a,2))-parseSize(cell(b,2)); return sortAsc?v:-v; }
        : function(a,b){ var v=cell(a,1).localeCompare(cell(b,1)); return sortAsc?v:-v; };
    d.sort(cmp); f.sort(cmp);
    var body = t.tBodies[0];
    d.concat(f).forEach(function(x){ body.appendChild(x); });
    /* Update indicators */
    [].slice.call(headers).forEach(function(h,i) {
        if (i===0) return;
        var txt = h.textContent.replace(/ [▲▼]$/,"");
        if (i===sortCol) txt += sortAsc ? " ▲" : " ▼";
        h.textContent = txt;
    });
}

if (headers.length >= 3) {
    headers[1].style.cursor = "pointer";
    headers[2].style.cursor = "pointer";
    headers[1].onclick = function() {
        if (sortCol===1) sortAsc=!sortAsc; else { sortCol=1; sortAsc=true; }
        doSort();
    };
    headers[2].onclick = function() {
        if (sortCol===2) sortAsc=!sortAsc; else { sortCol=2; sortAsc=true; }
        doSort();
    };
}
doSort();

/* ===== Filter ===== */
searchBox.oninput = function() {
    var q = searchBox.value.toLowerCase();
    rows().forEach(function(r) {
        r.style.display = cell(r,1).indexOf(q) >= 0 ? "" : "none";
    });
};

/* ===== Batch delete ===== */
batchBtn.onclick = function() {
    var items = [];
    [].slice.call(document.querySelectorAll(".sel:checked")).forEach(function(c) {
        var dl = c.closest("tr").querySelector("a[href*='/delete']");
        if (dl) items.push(dl.href);
    });
    if (!items.length) return;
    if (!confirm("Delete " + items.length + " item(s)?")) return;
    var i = 0;
    (function next() {
        if (i >= items.length) { location.reload(); return; }
        var x = new XMLHttpRequest();
        x.open("GET", items[i++]);
        x.onload = x.onerror = next;
        x.send();
    })();
};

/* ===== Universal file preview ===== */
var extMap = {
    /* text */
    txt:1,log:1,conf:1,cfg:1,ini:1,json:1,xml:1,csv:1,md:1,sh:1,py:1,js:1,css:1,html:1,
    yml:1,yaml:1,toml:1,env:1,bat:1,cmd:1,sql:1,sub:1,ir:1,
    /* image */
    png:2,jpg:2,jpeg:2,gif:2,bmp:2,svg:2,ico:2,webp:2,
    /* audio */
    mp3:3,wav:3,ogg:3,flac:3,m4a:3,aac:3,
    /* video */
    mp4:4,webm:4,mkv:4,avi:4,mov:4,
    /* pdf */
    pdf:5
};

function getExt(name) {
    var dot = name.lastIndexOf(".");
    return dot >= 0 ? name.substring(dot+1).toLowerCase() : "";
}

function getType(name) { return extMap[getExt(name)] || 0; }

/* Create modal overlay */
function makeModal(name) {
    var ov = document.createElement("div");
    ov.style.cssText = "position:fixed;top:0;left:0;width:100%;height:100%;" +
        "background:rgba(0,0,0,0.75);display:flex;align-items:center;justify-content:center;" +
        "z-index:9999;padding:12px;box-sizing:border-box";
    var box = document.createElement("div");
    box.style.cssText = "width:90%;max-width:800px;max-height:90vh;display:flex;" +
        "flex-direction:column;background:"+bg+";color:"+fg+";border:1px solid;overflow:hidden";
    var hdr = document.createElement("div");
    hdr.style.cssText = "padding:8px 12px;font-weight:bold;border-bottom:1px solid;" +
        "display:flex;justify-content:space-between;align-items:center;flex-shrink:0";
    var title = document.createElement("span");
    title.textContent = name;
    var close = document.createElement("span");
    close.innerHTML = "&#10005;";
    close.style.cssText = "cursor:pointer;padding:0 4px;font-size:18px";
    close.title = "Close";
    close.onclick = function(){ document.body.removeChild(ov); };
    hdr.appendChild(title); hdr.appendChild(close);
    box.appendChild(hdr);
    ov.appendChild(box);
    ov.onclick = function(e){ if(e.target===ov) document.body.removeChild(ov); };
    /* Escape key */
    var esc = function(e){ if(e.key==="Escape"){ document.body.removeChild(ov); document.removeEventListener("keydown",esc); }};
    document.addEventListener("keydown", esc);
    return {overlay: ov, box: box};
}

/* Text preview */
function previewText(url, name) {
    var x = new XMLHttpRequest();
    x.open("GET", url);
    x.onload = function() {
        var m = makeModal(name);
        var pre = document.createElement("pre");
        pre.style.cssText = "margin:0;padding:12px;overflow:auto;flex:1;white-space:pre-wrap;" +
            "word-break:break-all;font-family:inherit";
        var txt = x.responseText;
        if (txt.length > 32768) txt = txt.substring(0,32768) + "\n\n--- truncated at 32 KB ---";
        pre.textContent = txt;
        m.box.appendChild(pre);
        document.body.appendChild(m.overlay);
    };
    x.onerror = function(){ alert("Failed to load"); };
    x.send();
}

/* Binary preview (image/audio/video/pdf) via blob */
function previewBlob(url, name, type) {
    var x = new XMLHttpRequest();
    x.open("GET", url);
    x.responseType = "blob";
    x.onload = function() {
        var blob = x.response;
        var blobUrl = URL.createObjectURL(blob);
        var m = makeModal(name);
        var content = document.createElement("div");
        content.style.cssText = "flex:1;overflow:auto;display:flex;align-items:center;" +
            "justify-content:center;padding:8px;min-height:200px";
        var el;
        if (type === 2) {
            /* Image */
            el = document.createElement("img");
            el.src = blobUrl;
            el.style.cssText = "max-width:100%;max-height:80vh;object-fit:contain";
        } else if (type === 3) {
            /* Audio */
            el = document.createElement("audio");
            el.src = blobUrl;
            el.controls = true;
            el.style.width = "100%";
        } else if (type === 4) {
            /* Video */
            el = document.createElement("video");
            el.src = blobUrl;
            el.controls = true;
            el.style.cssText = "max-width:100%;max-height:75vh";
        } else if (type === 5) {
            /* PDF */
            el = document.createElement("iframe");
            el.src = blobUrl;
            el.style.cssText = "width:100%;height:75vh;border:none";
        }
        if (el) content.appendChild(el);
        m.box.appendChild(content);
        document.body.appendChild(m.overlay);
        /* Cleanup blob URL when modal closes */
        var origClose = m.overlay.querySelector("span[title='Close']").onclick;
        var cleanup = function(){ URL.revokeObjectURL(blobUrl); };
        m.overlay.querySelector("span[title='Close']").onclick = function(){ cleanup(); origClose(); };
        var origBg = m.overlay.onclick;
        m.overlay.onclick = function(e){ if(e.target===m.overlay){ cleanup(); document.body.removeChild(m.overlay); }};
    };
    x.onerror = function(){ alert("Failed to load"); };
    x.send();
}

/* Hex preview for unknown binary files */
function previewHex(url, name) {
    var x = new XMLHttpRequest();
    x.open("GET", url);
    x.responseType = "arraybuffer";
    x.onload = function() {
        var buf = new Uint8Array(x.response);
        var maxBytes = Math.min(buf.length, 4096);
        var lines = [];
        for (var i = 0; i < maxBytes; i += 16) {
            var addr = ("00000000" + i.toString(16)).slice(-8);
            var hex = [], ascii = [];
            for (var j = 0; j < 16; j++) {
                if (i+j < maxBytes) {
                    var b = buf[i+j];
                    hex.push(("0"+b.toString(16)).slice(-2));
                    ascii.push(b >= 32 && b < 127 ? String.fromCharCode(b) : ".");
                } else {
                    hex.push("  ");
                    ascii.push(" ");
                }
                if (j === 7) hex.push("");
            }
            lines.push(addr + "  " + hex.join(" ") + "  |" + ascii.join("") + "|");
        }
        if (buf.length > maxBytes) lines.push("\n--- showing first 4 KB of " + buf.length + " bytes ---");
        var m = makeModal(name + " (hex)");
        var pre = document.createElement("pre");
        pre.style.cssText = "margin:0;padding:12px;overflow:auto;flex:1;font-family:monospace;" +
            "font-size:12px;line-height:1.4;white-space:pre";
        pre.textContent = lines.join("\n");
        m.box.appendChild(pre);
        document.body.appendChild(m.overlay);
    };
    x.onerror = function(){ alert("Failed to load"); };
    x.send();
}

/* Attach preview to all file rows */
rows().forEach(function(r) {
    if (isDir(r)) return;
    var nc = r.cells[1]; if (!nc) return;
    var name = nc.textContent.trim();
    var type = getType(name);
    var dl = r.querySelector("a[href*='/download']");
    if (!dl) return;

    /* Type indicator */
    var icons = {1:"📄",2:"🖼",3:"🎵",4:"🎬",5:"📕",0:"🔢"};
    var badge = document.createElement("span");
    badge.textContent = icons[type] || icons[0];
    badge.style.cssText = "margin-right:4px;font-size:11px";
    nc.insertBefore(badge, nc.firstChild);

    nc.style.cursor = "pointer";
    nc.title = type ? "Preview" : "Hex view";
    nc.onclick = function(ev) {
        ev.preventDefault();
        if (type === 1) previewText(dl.href, name);
        else if (type >= 2 && type <= 5) previewBlob(dl.href, name, type);
        else previewHex(dl.href, name);
    };
});

/* ===== Mobile: responsive styles ===== */
var style = document.createElement("style");
style.textContent =
    "@media(max-width:600px){" +
    "h1{font-size:16px}" +
    ".uf{flex-direction:column!important;align-items:stretch!important}" +
    ".uf input[type=file]{max-width:100%!important}" +
    "table{font-size:12px}" +
    "td,th{padding:4px 3px!important}" +
    ".b.bs{padding:3px 6px!important;font-size:11px!important}" +
    "}";
document.head.appendChild(style);

})();
