# WiFi

Documentar topologia, credenciales mediante configuracion local, reconexion y resultados.

Wifi fue muy similar a esp-now debido a que en teoria usan el mismo metodo de comunicacion. Esta prueba fue de distancia por que no tuvimos nada similar a una cisterna lleno de agua. La distancia fue de como 50 metros Punto A Sala de proyectos 6J Punto b Entrada del Cafeteria

Fui caminando desde Punto A Hacia Punto B y cuando llege a punto B Se perdio la conexion despues de 30 segundos

Datos Crudos

- I (67498) wifi:WPA3 SAE d4:05:92:79:3e:ec start reauthentication
- I (67498) wifi softAP: station d4:05:92:79:3e:ec leave, AID=1, reason=2
- I (68158) wifi softAP: station d4:05:92:79:3e:ec leave, AID=1, reason=2
- I (68158) wifi:<ba-del>idx:2, tid:0
- I (68158) wifi:new:<1,1>, old:<1,1>, ap:<1,1>, sta:<255,255>, prof:1, snd_ch_cfg:0x0
- I (68168) wifi:station: d4:05:92:79:3e:ec join, AID=1, bgn, 40U
- I (68188) wifi softAP: station d4:05:92:79:3e:ec join, AID=1
- I (68228) esp_netif_lwip: DHCP server assigned IP to a client, IP is: 192.168.4.2
- I (69248) wifi:<ba-add>idx:2 (ifx:1, d4:05:92:79:3e:ec), tid:0, ssn:0, winSize:64
- I (69248) wifi softAP: Recibido <- Seq:0 | RSSI:-39 dBm
- I (71238) wifi softAP: Recibido <- Seq:1 | RSSI:-45 dBm
- I (73238) wifi softAP: Recibido <- Seq:2 | RSSI:-41 dBm
- I (75238) wifi softAP: Recibido <- Seq:3 | RSSI:-43 dBm
- I (77238) wifi softAP: Recibido <- Seq:4 | RSSI:-39 dBm
- I (79238) wifi softAP: Recibido <- Seq:5 | RSSI:-47 dBm
- I (81238) wifi softAP: Recibido <- Seq:6 | RSSI:-56 dBm
- I (83238) wifi softAP: Recibido <- Seq:7 | RSSI:-43 dBm
- I (85248) wifi softAP: Recibido <- Seq:8 | RSSI:-44 dBm
- I (87248) wifi softAP: Recibido <- Seq:9 | RSSI:-47 dBm
- I (89238) wifi softAP: Recibido <- Seq:10 | RSSI:-48 dBm
- I (91238) wifi softAP: Recibido <- Seq:11 | RSSI:-57 dBm
- I (93238) wifi softAP: Recibido <- Seq:12 | RSSI:-65 dBm
- I (95238) wifi softAP: Recibido <- Seq:13 | RSSI:-62 dBm
- I (97238) wifi softAP: Recibido <- Seq:14 | RSSI:-64 dBm
- I (99238) wifi softAP: Recibido <- Seq:15 | RSSI:-61 dBm
- I (101238) wifi softAP: Recibido <- Seq:16 | RSSI:-73 dBm
- I (103238) wifi softAP: Recibido <- Seq:17 | RSSI:-68 dBm
- I (105258) wifi softAP: Recibido <- Seq:18 | RSSI:-81 dBm
- I (107258) wifi softAP: Recibido <- Seq:19 | RSSI:-81 dBm
- I (109248) wifi softAP: Recibido <- Seq:20 | RSSI:-82 dBm
- I (111248) wifi softAP: Recibido <- Seq:21 | RSSI:-80 dBm
- I (113238) wifi softAP: Recibido <- Seq:22 | RSSI:-71 dBm
- I (115238) wifi softAP: Recibido <- Seq:23 | RSSI:-66 dBm
- I (117238) wifi softAP: Recibido <- Seq:24 | RSSI:-66 dBm
- I (119238) wifi softAP: Recibido <- Seq:25 | RSSI:-69 dBm
- I (121248) wifi softAP: Recibido <- Seq:26 | RSSI:-65 dBm
- I (123258) wifi softAP: Recibido <- Seq:27 | RSSI:-71 dBm
- I (125238) wifi softAP: Recibido <- Seq:28 | RSSI:-66 dBm
- I (127258) wifi softAP: Recibido <- Seq:29 | RSSI:-75 dBm
- I (129238) wifi softAP: Recibido <- Seq:30 | RSSI:-67 dBm
- I (131238) wifi softAP: Recibido <- Seq:31 | RSSI:-70 dBm
- I (133238) wifi softAP: Recibido <- Seq:32 | RSSI:-86 dBm
- I (135258) wifi softAP: Recibido <- Seq:33 | RSSI:-85 dBm
- I (137248) wifi softAP: Recibido <- Seq:34 | RSSI:-89 dBm
- I (139258) wifi softAP: Recibido <- Seq:35 | RSSI:-91 dBm
- I (141338) wifi softAP: Recibido <- Seq:36 | RSSI:-95 dBm
- I (143308) wifi softAP: Recibido <- Seq:37 | RSSI:-89 dBm
- I (145268) wifi softAP: Recibido <- Seq:38 | RSSI:-92 dBm
W (149618) wifi softAP: ⚠️ ALERTA: Se perdieron 1 paquete(s) intermedio(s)
- I (149618) wifi softAP: Recibido <- Seq:40 | RSSI:-89 dBm
- I (151268) wifi softAP: Recibido <- Seq:41 | RSSI:-89 dBm
- I (153278) wifi softAP: Recibido <- Seq:42 | RSSI:-89 dBm
W (159538) wifi softAP: ⚠️ ALERTA: Se perdieron 2 paquete(s) intermedio(s)
- I (159538) wifi softAP: Recibido <- Seq:45 | RSSI:-94 dBm
W (163568) wifi softAP: ⚠️ ALERTA: Se perdieron 1 paquete(s) intermedio(s)
- I (163568) wifi softAP: Recibido <- Seq:47 | RSSI:-92 dBm
W (167588) wifi softAP: ⚠️ ALERTA: Se perdieron 1 paquete(s) intermedio(s)
- I (167588) wifi softAP: Recibido <- Seq:49 | RSSI:-92 dBm
W (177538) wifi softAP: ⚠️ ALERTA: Se perdieron 4 paquete(s) intermedio(s)
- I (177538) wifi softAP: Recibido <- Seq:54 | RSSI:-94 dBm
