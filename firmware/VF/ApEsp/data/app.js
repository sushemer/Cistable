const $ = (id) => document.getElementById(id);

function risk(value) {
  const absolute = Math.abs(value);
  if (absolute >= 0.8) return "CRITICO";
  if (absolute >= 0.35) return "PRECAUCION";
  return "SEGURO";
}

async function refresh() {
  try {
    const statusResponse = await fetch("/api/status", { cache: "no-store" });
    const status = await statusResponse.json();
    const master = status.master;
    const slave = status.slave;
    const active = master.online ? master : slave;

    $("ltr").textContent = active.ltr.toFixed(3);
    $("risk").textContent = risk(active.ltr);
    $("speed").textContent = `${(active.speed || 0).toFixed(1)} km/h`;
    $("water").textContent = (slave.water || 0).toFixed(1);
    $("nodes").textContent =
      `Cabina: ${master.online ? "ONLINE" : "OFFLINE"} ` +
      `roll=${master.roll.toFixed(1)} pitch=${master.pitch.toFixed(1)} ` +
      `ay=${master.ay.toFixed(2)}\n` +
      `Cisterna: ${slave.online ? "ONLINE" : "OFFLINE"} ` +
      `agua=${slave.water.toFixed(1)}`;

    const historyResponse = await fetch("/api/history", { cache: "no-store" });
    const history = await historyResponse.json();
    $("history").innerHTML = history.map((item) => `
      <tr>
        <td>${item.node == 1 ? "Cabina" : "Cisterna"}</td>
        <td>${item.ltr.toFixed(3)}</td>
        <td>${item.water.toFixed(1)}</td>
        <td>${item.speed.toFixed(1)}</td>
      </tr>
    `).join("");
  } catch (error) {
    console.error("Error de telemetria", error);
  }
}

async function command(command, payload) {
  const packet = new Uint8Array(4 + payload.length);
  packet[0] = 58;
  packet[1] = command;
  packet[2] = payload.length;
  packet.set(payload, 3);

  let crc = 0;
  for (let i = 0; i < packet.length - 1; i++) {
    crc ^= packet[i];
  }
  packet[packet.length - 1] = crc;

  const response = await fetch("/api/command", {
    method: "POST",
    headers: { "Content-Type": "application/octet-stream" },
    body: packet,
  });

  if (!response.ok) {
    throw new Error(await response.text());
  }
  return response.text();
}

function setStatus(message, isError = false) {
  $("command-status").textContent = message;
  $("command-status").classList.toggle("error", isError);
}

$("rate").onchange = (event) => {
  const payload = new Uint8Array(5);
  new DataView(payload.buffer).setUint32(0, Number(event.target.value), true);
  payload[4] = Number($("node").value);
  setStatus("Aplicando intervalo...");
  command(24, payload)
    .then(() => setStatus("Intervalo enviado al nodo seleccionado"))
    .catch((error) => setStatus(error.message, true));
};

$("enable").onclick = () => {
  const node = Number($("node").value);
  setStatus("Habilitando reportes...");
  command(76, new Uint8Array([node, 1]))
    .then(() => setStatus("Reportes del nodo habilitados"))
    .catch((error) => setStatus(error.message, true));
};

$("disable").onclick = () => {
  const node = Number($("node").value);
  setStatus("Deshabilitando reportes...");
  command(76, new Uint8Array([node, 0]))
    .then(() => setStatus("Reportes del nodo deshabilitados"))
    .catch((error) => setStatus(error.message, true));
};

$("sleep").onclick = () => {
  const node = Number($("node").value);
  const nodeName = node === 1 ? "Cabina" : "Cisterna";
  const button = $("sleep");
  button.disabled = true;
  setStatus(`Enviando ahorro de 10 s a ${nodeName}...`);

  command(158, new Uint8Array([node, 10]))
    .then(() => {
      setStatus(`Ahorro enviado a ${nodeName}. El nodo reiniciara y reconectara despues de 10 s.`);
      window.setTimeout(() => {
        button.disabled = false;
        setStatus("Periodo de ahorro completado. Esperando la reconexion del nodo.");
      }, 11000);
    })
    .catch((error) => {
      button.disabled = false;
      setStatus(error.message, true);
    });
};

refresh();
setInterval(refresh, 2000);
