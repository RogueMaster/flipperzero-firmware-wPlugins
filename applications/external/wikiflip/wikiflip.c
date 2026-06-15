#include "wikiflip.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>
#include <stdlib.h>

struct WikiFlipApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* categories_menu;
    Submenu* terms_menu;
    Widget* definition_widget;
    FuriString* definition_buffer;
    const WikiFlipCategory* current_category;
    uint32_t current_category_index;
    uint32_t current_term_index;
    WikiFlipView current_view;
};

static const WikiFlipTerm wikiflip_protocolos_terms[] = {
    {
        .title = "TLS 1.3",
        .definition =
            "Transport Layer Security 1.3 protege canales TCP con cifrado autenticado, "
            "intercambio de claves efimero y suites AEAD. Elimina RSA key exchange, CBC "
            "y renegociacion heredada.\n\n"
            "Auditoria: validar certificados, SNI, ALPN, versiones minimas, HSTS, cipher "
            "suites, OCSP stapling y bloqueo de downgrade.",
    },
    {
        .title = "DNSSEC",
        .definition =
            "DNSSEC agrega autenticidad e integridad a DNS mediante firmas digitales, "
            "DNSKEY, DS y RRSIG. No cifra consultas; evita respuestas falsificadas cuando "
            "el resolver valida la cadena de confianza.\n\n"
            "Auditoria: revisar DS padre, rollover KSK/ZSK, expiracion RRSIG, NSEC/NSEC3 "
            "y fallos de validacion.",
    },
    {
        .title = "SSH",
        .definition =
            "SSH provee administracion remota cifrada, autenticacion por clave publica y "
            "tuneles TCP. La superficie critica incluye politicas de login, algoritmos, "
            "manejo de claves y port forwarding.\n\n"
            "Auditoria: bloquear root directo, limitar password auth, revisar authorized_keys, "
            "KEX debiles, MFA y bastion host.",
    },
    {
        .title = "HTTP/2",
        .definition =
            "HTTP/2 usa multiplexacion, compresion HPACK y streams sobre una sola conexion. "
            "Reduce latencia pero introduce riesgos de abuso de streams, request smuggling "
            "y agotamiento de recursos.\n\n"
            "Auditoria: validar limites de streams, proxies intermedios, normalizacion de "
            "headers y comportamiento HTTP/1.1 hacia backend.",
    },
    {
        .title = "HTTP/3 QUIC",
        .definition =
            "HTTP/3 usa QUIC sobre UDP con TLS 1.3 integrado. Mejora latencia y movilidad "
            "de conexion, pero cambia telemetria, inspeccion y controles de red tradicionales.\n\n"
            "Auditoria: validar politica UDP/443, fallback, certificados, logging, rate limiting "
            "y visibilidad en WAF/CDN.",
    },
    {
        .title = "OAuth 2.0",
        .definition =
            "OAuth 2.0 delega autorizacion mediante access tokens, scopes y grants. No es "
            "autenticacion por si mismo; para identidad debe complementarse con OpenID Connect.\n\n"
            "Auditoria: revisar redirect_uri, PKCE, scopes, expiracion, audience, token storage "
            "y grants inseguros.",
    },
    {
        .title = "OpenID Connect",
        .definition =
            "OIDC extiende OAuth 2.0 para autenticacion federada usando ID tokens firmados, "
            "claims, discovery y JWKS. Es comun en SSO moderno.\n\n"
            "Auditoria: validar issuer, audience, nonce, firma, alg none, rotacion JWKS, "
            "claims sensibles y logout.",
    },
    {
        .title = "SAML 2.0",
        .definition =
            "SAML 2.0 permite federacion empresarial basada en assertions XML firmadas entre "
            "Identity Provider y Service Provider.\n\n"
            "Auditoria: comprobar firma obligatoria, canonicalizacion XML, audience, recipient, "
            "NameID, expiracion y proteccion contra XML signature wrapping.",
    },
    {
        .title = "Kerberos",
        .definition =
            "Kerberos autentica entidades mediante tickets emitidos por un KDC. En Windows AD "
            "sostiene SSO, delegacion, SPN y tickets TGT/TGS.\n\n"
            "Auditoria: revisar Kerberoasting, AS-REP roasting, delegacion unconstrained, SPN "
            "expuestos, clock skew y politicas de contrasena.",
    },
    {
        .title = "LDAP/LDAPS",
        .definition =
            "LDAP consulta directorios; LDAPS agrega TLS. En AD se usa para identidades, grupos, "
            "atributos y autenticacion de aplicaciones.\n\n"
            "Auditoria: exigir signing/channel binding, bloquear binds anonimos, usar LDAPS, "
            "limitar cuentas de servicio y revisar filtros LDAP injection.",
    },
    {
        .title = "SMB",
        .definition =
            "Server Message Block permite archivos compartidos, named pipes y administracion "
            "remota en Windows. SMBv1 es obsoleto y de alto riesgo.\n\n"
            "Auditoria: deshabilitar SMBv1, exigir signing donde aplique, revisar shares, "
            "permisos NTFS, sesiones anonimas y exposicion 445/TCP.",
    },
    {
        .title = "MQTT",
        .definition =
            "MQTT es mensajeria publish/subscribe ligera usada en IoT. Usa brokers, topics, QoS "
            "y clientes con credenciales o certificados.\n\n"
            "Auditoria: revisar TLS, autenticacion, ACL por topic, retained messages, anonymous "
            "access, bridge configs y fuga de telemetria.",
    },
};

static const WikiFlipTerm wikiflip_owasp_terms[] = {
    {
        .title = "A01:2025 - Broken Access Control",
        .definition =
            "Fallo al imponer autorizacion de forma consistente. Incluye IDOR, escalacion "
            "horizontal o vertical, bypass de endpoints, abuso de metadatos, CORS inseguro, "
            "SSRF consolidado en esta categoria y controles aplicados solo en cliente.\n\n"
            "Mitigacion: deny by default, autorizacion server-side centralizada, controles por "
            "objeto, ownership checks, pruebas por rol, rate limiting y logs de fallos de acceso.",
    },
    {
        .title = "A02:2025 - Security Misconfiguration",
        .definition =
            "Configuraciones inseguras en aplicaciones, cloud, contenedores, servidores o "
            "frameworks. Incluye defaults, servicios innecesarios, permisos amplios, debug, "
            "headers ausentes, CORS permisivo, errores verbosos y storage publico.\n\n"
            "Mitigacion: hardening automatizado, IaC versionado, baselines, configuracion minima, "
            "secret scanning y validacion continua.",
    },
    {
        .title = "A03:2025 - Software Supply Chain Failures",
        .definition =
            "Riesgos introducidos por dependencias, build systems, paquetes, pipelines, "
            "repositorios, artefactos, firmas, actualizaciones y proveedores. Amplia el enfoque "
            "mas alla de componentes vulnerables conocidos.\n\n"
            "Mitigacion: SBOM, pinning, SCA, repositorios confiables, firmas, provenance, revision "
            "de CI/CD, least privilege en tokens y control de releases.",
    },
    {
        .title = "A04:2025 - Cryptographic Failures",
        .definition =
            "Uso incorrecto o ausente de criptografia que termina exponiendo datos o debilitando "
            "confianza. Incluye algoritmos obsoletos, claves debiles, TLS mal configurado, "
            "almacenamiento en claro y manejo deficiente de secretos.\n\n"
            "Mitigacion: criptografia moderna, gestion de claves, rotacion, KMS/HSM, TLS fuerte, "
            "hashing de contrasenas con sal y revisiones de diseno.",
    },
    {
        .title = "A05:2025 - Injection",
        .definition =
            "Datos no confiables alteran consultas, comandos, interpretes o plantillas. Incluye "
            "SQL, NoSQL, OS command, LDAP, XPath, template injection y XSS cuando el contexto lo "
            "permite.\n\n"
            "Mitigacion: parametros preparados, validacion positiva, escaping contextual, ORMs "
            "bien usados, sandboxing y separacion estricta entre datos y codigo.",
    },
    {
        .title = "A06:2025 - Insecure Design",
        .definition =
            "Defectos de arquitectura o logica que no se corrigen con parches puntuales. Incluye "
            "flujos de negocio abusables, falta de controles compensatorios, limites ausentes y "
            "amenazas no modeladas.\n\n"
            "Mitigacion: threat modeling, secure design reviews, abuso de casos, requisitos de "
            "seguridad verificables y patrones defensivos desde el diseno.",
    },
    {
        .title = "A07:2025 - Authentication Failures",
        .definition =
            "Debilidades en identidad, login, sesiones, MFA y recuperacion de cuenta. Incluye "
            "credential stuffing, contrasenas debiles, reset inseguro, sesiones predecibles y "
            "validacion incompleta de tokens.\n\n"
            "Mitigacion: MFA resistente, rate limiting, hashing fuerte, sesiones seguras, rotacion "
            "de tokens, deteccion de abuso y procesos robustos de recuperacion.",
    },
    {
        .title = "A08:2025 - Software or Data Integrity Failures",
        .definition =
            "Confianza indebida en software, actualizaciones, datos, serializacion o pipelines sin "
            "verificar integridad. Incluye CI/CD comprometido, plugins no confiables, deserializacion "
            "insegura y datos criticos sin firma.\n\n"
            "Mitigacion: firmas, verificacion de artefactos, controles de pipeline, allowlists, "
            "deserializacion segura y control de cambios.",
    },
    {
        .title = "A09:2025 - Security Logging and Alerting Failures",
        .definition =
            "Registro, monitoreo o alerta insuficiente para detectar, investigar y responder. "
            "Incluye eventos criticos ausentes, logs no correlacionables, alertas sin contexto y "
            "retencion inadecuada.\n\n"
            "Mitigacion: eventos de seguridad definidos, IDs de correlacion, SIEM, alertas con "
            "severidad, proteccion de logs, retencion y pruebas de deteccion.",
    },
    {
        .title = "A10:2025 - Mishandling of Exceptional Conditions",
        .definition =
            "Manejo incorrecto de errores, excepciones, timeouts, estados parciales y fallos de "
            "dependencias. Puede provocar fugas de informacion, crashes, bypass de controles, "
            "inconsistencia de datos o denegacion de servicio.\n\n"
            "Mitigacion: fail secure, errores genericos al usuario, logging interno seguro, "
            "timeouts, retries controlados, circuit breakers y pruebas de caos controladas.",
    },
};

static const WikiFlipTerm wikiflip_nist_terms[] = {
    {
        .title = "NIST CSF 2.0",
        .definition =
            "Framework de ciberseguridad con funciones Govern, Identify, Protect, Detect, Respond "
            "y Recover. Sirve para madurez, comunicacion de riesgo y priorizacion.\n\n"
            "Auditoria: mapear controles, brechas, perfiles actuales/objetivo, riesgos y acciones "
            "medibles por funcion.",
    },
    {
        .title = "NIST SP 800-53",
        .definition =
            "Catalogo de controles de seguridad y privacidad. Incluye familias AC, AU, CM, IA, "
            "IR, RA, SC, SI y otras.\n\n"
            "Auditoria: seleccionar baseline, levantar evidencias, probar implementacion, registrar "
            "excepciones y relacionar controles con riesgos.",
    },
    {
        .title = "NIST SP 800-61",
        .definition =
            "Guia de manejo de incidentes: preparacion, deteccion, analisis, contencion, "
            "erradicacion, recuperacion y post-incidente.\n\n"
            "Auditoria: revisar playbooks, severidad, cadena de custodia, comunicaciones, tiempos "
            "de respuesta y lecciones aprendidas.",
    },
    {
        .title = "NIST SP 800-30",
        .definition =
            "Guia para evaluacion de riesgos. Cubre amenazas, vulnerabilidades, probabilidad, "
            "impacto, controles existentes y recomendacion de tratamiento.\n\n"
            "Auditoria: comprobar metodologia, criterios repetibles, trazabilidad de activos y "
            "priorizacion basada en riesgo.",
    },
    {
        .title = "NIST SP 800-37 RMF",
        .definition =
            "Risk Management Framework para categorizar, seleccionar, implementar, evaluar, "
            "autorizar y monitorear controles.\n\n"
            "Auditoria: revisar autorizacion, herencia de controles, POA&M, monitoreo continuo y "
            "cambios significativos.",
    },
    {
        .title = "NIST SP 800-63",
        .definition =
            "Guia de identidad digital con niveles IAL, AAL y FAL. Define criterios para prueba "
            "de identidad, autenticadores y federacion.\n\n"
            "Auditoria: evaluar MFA, phishing resistance, lifecycle de credenciales, recovery y "
            "claims federados.",
    },
    {
        .title = "NIST SP 800-171",
        .definition =
            "Proteccion de Controlled Unclassified Information en sistemas no federales. Muy usado "
            "en entornos de cadena de suministro gubernamental.\n\n"
            "Auditoria: revisar controles, SSP, POA&M, acceso, configuracion, incidentes y manejo "
            "de CUI.",
    },
    {
        .title = "NIST SP 800-207",
        .definition =
            "Arquitectura Zero Trust basada en verificacion continua, politicas dinamicas y acceso "
            "minimo por sesion.\n\n"
            "Auditoria: revisar policy engine, identity, posture, segmentacion, telemetria y "
            "decisiones contextuales.",
    },
    {
        .title = "NIST SP 800-218 SSDF",
        .definition =
            "Secure Software Development Framework. Organiza practicas para preparar la organizacion, "
            "proteger software, producir software seguro y responder a vulnerabilidades.\n\n"
            "Auditoria: mapear SDLC, code review, SCA, threat modeling, secrets, builds y respuesta.",
    },
    {
        .title = "FIPS 140-3",
        .definition =
            "Estandar para validacion de modulos criptograficos. Define niveles y requisitos para "
            "algoritmos, roles, autenticacion, fisico y self-tests.\n\n"
            "Auditoria: verificar uso de modulos validados, modo FIPS, certificados vigentes y "
            "configuracion real.",
    },
};

static const WikiFlipTerm wikiflip_cve_terms[] = {
    {
        .title = "CVE-2021-44228 Log4Shell",
        .definition =
            "RCE critica en Apache Log4j 2 por lookups JNDI controlables por atacante. Impacto "
            "alto por uso transitivo en aplicaciones Java.\n\n"
            "Validacion: versiones afectadas, dependencias indirectas, patrones en logs, egress y "
            "IoC historicos. Mitigacion: actualizar y restringir JNDI/egress.",
    },
    {
        .title = "CVE-2017-0144 EternalBlue",
        .definition =
            "Vulnerabilidad SMBv1 en Windows asociada a MS17-010. Permite ejecucion remota en "
            "sistemas sin parche con 445/TCP expuesto.\n\n"
            "Mitigacion: aplicar parches, deshabilitar SMBv1, segmentar y bloquear SMB no necesario.",
    },
    {
        .title = "CVE-2023-34362 MOVEit",
        .definition =
            "Fallo critico en MOVEit Transfer explotado para acceso no autenticado y exfiltracion "
            "de datos.\n\n"
            "Validacion: version, webshells, logs, transferencias inusuales y cuentas creadas. "
            "Mitigacion: parches, aislamiento y rotacion de credenciales.",
    },
    {
        .title = "CVE-2024-3094 XZ Utils",
        .definition =
            "Backdoor introducido en versiones especificas de XZ Utils que afectaba integridad de "
            "la cadena de suministro y podia impactar sshd en ciertos builds Linux.\n\n"
            "Mitigacion: retirar versiones afectadas, verificar paquetes, revisar provenance y "
            "fortalecer controles de supply chain.",
    },
    {
        .title = "CVE-2023-23397 Outlook",
        .definition =
            "Elevacion de privilegios en Microsoft Outlook por procesamiento de recordatorios que "
            "pueden forzar autenticacion NTLM hacia recursos controlados por atacante.\n\n"
            "Mitigacion: parches, bloqueo NTLM saliente, deteccion de propiedades MAPI anormales "
            "y monitoreo de autenticacion externa.",
    },
    {
        .title = "CVE-2023-46805 Ivanti",
        .definition =
            "Bypass de autenticacion en Ivanti Connect Secure/Policy Secure, observado en cadenas "
            "de explotacion con otras fallas para ejecucion remota.\n\n"
            "Mitigacion: aplicar parches, ejecutar integrity checker, revisar web roots, sesiones "
            "admin y accesos VPN inusuales.",
    },
    {
        .title = "CVE-2024-21887 Ivanti",
        .definition =
            "Command injection en Ivanti Connect Secure/Policy Secure explotable por usuarios "
            "autenticados o en cadena con bypass previo.\n\n"
            "Mitigacion: actualizar, aislar administracion, revisar logs, artefactos persistentes "
            "y credenciales expuestas.",
    },
    {
        .title = "CVE-2024-6387 regreSSHion",
        .definition =
            "Condicion de carrera en OpenSSH server relacionada con signal handler async-unsafe. "
            "Puede permitir RCE pre-auth en ciertas plataformas y versiones.\n\n"
            "Mitigacion: actualizar OpenSSH, limitar exposicion, aplicar LoginGraceTime conservador "
            "y filtrar acceso SSH.",
    },
    {
        .title = "CVE-2023-3519 Citrix ADC",
        .definition =
            "RCE no autenticada en Citrix ADC/Gateway ampliamente explotada contra appliances "
            "expuestos.\n\n"
            "Validacion: version, webshells, sesiones, logs HTTP y cambios de configuracion. "
            "Mitigacion: parche inmediato y hunting.",
    },
    {
        .title = "CVE-2020-1472 Zerologon",
        .definition =
            "Debilidad en Netlogon Remote Protocol que permite tomar control de un domain "
            "controller bajo condiciones vulnerables.\n\n"
            "Mitigacion: parches, enforcement mode, monitoreo de Netlogon, eventos DC y rotacion "
            "si hubo compromiso.",
    },
    {
        .title = "CVE-2022-30190 Follina",
        .definition =
            "Abuso de Microsoft Support Diagnostic Tool mediante documentos o enlaces que invocan "
            "ms-msdt para ejecucion de comandos.\n\n"
            "Mitigacion: parches, bloqueo de protocolo, reglas EDR y deteccion de procesos Office "
            "lanzando msdt/script interpreters.",
    },
    {
        .title = "CVE-2024-1709 ScreenConnect",
        .definition =
            "Authentication bypass en ConnectWise ScreenConnect que permite acceso administrativo "
            "o takeover de instancia vulnerable.\n\n"
            "Mitigacion: actualizar, revisar usuarios, sesiones, extensiones, logs y hosts remotos "
            "administrados.",
    },
};

static const WikiFlipTerm wikiflip_blueteam_terms[] = {
    {
        .title = "SIEM",
        .definition =
            "Centraliza logs, normaliza eventos, correlaciona senales y genera alertas. Su valor "
            "depende de cobertura, parsing, casos de uso y respuesta operativa.\n\n"
            "Buenas practicas: inventario de fuentes, tuning, retencion, dashboards y reglas con "
            "contexto.",
    },
    {
        .title = "EDR",
        .definition =
            "Monitorea procesos, memoria, red, persistencia y cambios en endpoints. Permite "
            "aislamiento, recoleccion de evidencias y hunting.\n\n"
            "Auditoria: cobertura, exclusions, tamper protection, sensores caidos y acciones de "
            "respuesta remota.",
    },
    {
        .title = "YARA",
        .definition =
            "Lenguaje para describir patrones de archivos o memoria con strings, condiciones y "
            "modulos. Util para malware triage y busqueda forense.\n\n"
            "Practica: reglas especificas, metadatos, pruebas de falsos positivos y versionado.",
    },
    {
        .title = "Sigma",
        .definition =
            "Formato generico para reglas de deteccion sobre logs. Permite convertir una logica "
            "a multiples SIEMs y backend queries.\n\n"
            "Practica: mapear campos, definir logsource, probar conversion y documentar falsos "
            "positivos esperados.",
    },
    {
        .title = "SOAR",
        .definition =
            "Security Orchestration, Automation and Response automatiza playbooks, enriquecimiento, "
            "tickets y acciones de contencion.\n\n"
            "Auditoria: revisar aprobaciones, limites, rollback, secretos, integraciones y manejo "
            "de errores.",
    },
    {
        .title = "UEBA",
        .definition =
            "User and Entity Behavior Analytics detecta anomalias comparando actividad contra "
            "baselines de usuarios, hosts o servicios.\n\n"
            "Uso: detectar abuso de credenciales, horarios anormales, viajes imposibles y cambios "
            "en volumen de acceso.",
    },
    {
        .title = "DFIR",
        .definition =
            "Digital Forensics and Incident Response combina preservacion de evidencia, analisis "
            "forense y respuesta tecnica.\n\n"
            "Practica: cadena de custodia, triage, timeline, memoria, disco, logs y reporte "
            "ejecutivo/tecnico.",
    },
    {
        .title = "Windows 4624/4625",
        .definition =
            "Eventos de inicio de sesion exitoso y fallido. Claves para detectar password spraying, "
            "lateral movement y accesos fuera de patron.\n\n"
            "Campos: LogonType, AccountName, WorkstationName, IpAddress, AuthenticationPackage y "
            "Status/SubStatus.",
    },
    {
        .title = "Sysmon",
        .definition =
            "Componente de Sysinternals que registra telemetria detallada de procesos, red, DNS, "
            "drivers, archivos y registry.\n\n"
            "Practica: usar config mantenida, filtrar ruido, correlacionar con EDR y proteger la "
            "integridad de logs.",
    },
    {
        .title = "NDR",
        .definition =
            "Network Detection and Response analiza trafico para detectar beaconing, C2, exfiltracion, "
            "movimiento lateral y protocolos raros.\n\n"
            "Auditoria: cobertura SPAN/TAP, TLS metadata, DNS, NetFlow, Zeek y respuesta a alertas.",
    },
    {
        .title = "Incident Playbook",
        .definition =
            "Procedimiento repetible para responder a un escenario: ransomware, phishing, BEC, "
            "credenciales filtradas o host comprometido.\n\n"
            "Debe incluir criterios de severidad, roles, contencion, evidencias, comunicacion y "
            "recuperacion.",
    },
    {
        .title = "Threat Hunting",
        .definition =
            "Busqueda proactiva basada en hipotesis sobre actividad adversaria no alertada. Usa "
            "telemetria, TTP, estadistica y conocimiento del entorno.\n\n"
            "Salida: hallazgos, nuevas detecciones, gaps de logging y mejoras de controles.",
    },
};

static const WikiFlipTerm wikiflip_redteam_terms[] = {
    {
        .title = "Phishing Simulation",
        .definition =
            "Ejercicio controlado para medir correo, usuarios y proceso de reporte. Mide entrega, "
            "apertura, clic, credenciales evitadas y reporte.\n\n"
            "Evalua SPF, DKIM, DMARC, sandboxing, banners, bloqueo URL y respuesta SOC.",
    },
    {
        .title = "Lateral Movement",
        .definition =
            "Movimiento entre sistemas despues de acceso inicial. Puede abusar sesiones, shares, "
            "RDP, WinRM, WMI, SSH, Kerberos o herramientas administrativas.\n\n"
            "Controles: segmentacion, privilegios locales, deteccion y proteccion de credenciales.",
    },
    {
        .title = "C2",
        .definition =
            "Command and Control permite enviar tareas y recibir resultados de implantes en una "
            "prueba autorizada. Puede usar HTTPS, DNS, mTLS, redirectors y perfiles.\n\n"
            "Evalua egress filtering, TLS inspection, DNS y deteccion de beaconing.",
    },
    {
        .title = "Initial Access",
        .definition =
            "Fase de entrada al entorno: credenciales, VPN, phishing, exposicion publica, supply "
            "chain o servicios vulnerables.\n\n"
            "Auditoria: medir superficie real, controles preventivos y trazabilidad de deteccion.",
    },
    {
        .title = "Privilege Escalation",
        .definition =
            "Obtencion de mayores permisos en host, dominio, cloud o aplicacion. Incluye servicios "
            "mal configurados, permisos debiles, kernels vulnerables y secretos locales.\n\n"
            "Mitigacion: hardening, least privilege, patching y monitoreo de cambios privilegiados.",
    },
    {
        .title = "Persistence",
        .definition =
            "Mecanismos para mantener acceso: servicios, scheduled tasks, run keys, startup items, "
            "OAuth apps, webshells o cuentas creadas.\n\n"
            "Blue Team: monitorear creacion/modificacion y establecer baselines.",
    },
    {
        .title = "Credential Access",
        .definition =
            "Tecnicas para obtener secretos: LSASS, dumps, browser stores, keychains, configs, "
            "tokens cloud, Kerberos y password spraying.\n\n"
            "Controles: MFA, Credential Guard, secret scanning, vaulting y deteccion.",
    },
    {
        .title = "Kerberoasting",
        .definition =
            "Solicitud de tickets TGS para cuentas con SPN y cracking offline del hash del servicio. "
            "No requiere privilegios elevados en AD tradicional.\n\n"
            "Mitigacion: contrasenas largas, gMSA, AES, monitoreo de TGS anomalo y limpieza de SPN.",
    },
    {
        .title = "Pass-the-Hash",
        .definition =
            "Uso de hashes NTLM sin conocer la contrasena en claro para autenticarse lateralmente. "
            "Impacta entornos con NTLM habilitado y privilegios reutilizados.\n\n"
            "Mitigacion: limitar admins locales, LAPS, segmentacion, bloqueo NTLM y EDR.",
    },
    {
        .title = "Living off the Land",
        .definition =
            "Uso de binarios y herramientas legitimas del sistema para ejecucion, descarga, "
            "persistencia o evasiones.\n\n"
            "Ejemplos: PowerShell, certutil, mshta, rundll32, wmic, bitsadmin y schtasks.",
    },
    {
        .title = "OPSEC",
        .definition =
            "Disciplina operacional para reducir ruido, preservar trazabilidad autorizada y evitar "
            "impacto. Incluye horarios, volumen, payloads, infraestructura y manejo de artefactos.\n\n"
            "En auditoria formal se documenta cada accion critica y rollback.",
    },
    {
        .title = "Payload Staging",
        .definition =
            "Separacion entre un primer cargador pequeno y componentes posteriores. Reduce tamano "
            "inicial pero aumenta dependencias de red y senales observables.\n\n"
            "Controles: bloqueo egress, inspeccion, EDR y allowlisting.",
    },
};

static const WikiFlipTerm wikiflip_threat_intelligence_terms[] = {
    {
        .title = "IOC",
        .definition =
            "Indicator of Compromise: hash, IP, dominio, ruta, mutex, certificado o patron de log "
            "asociado a actividad maliciosa.\n\n"
            "Gestion: fuente, fecha, confianza, contexto, expiracion y correlacion con TTP.",
    },
    {
        .title = "TTP",
        .definition =
            "Tactics, Techniques and Procedures describen comportamiento adversario. Son mas "
            "duraderos que IoC y se mapean comunmente a MITRE ATT&CK.\n\n"
            "Uso: emulacion, deteccion, gap analysis y priorizacion defensiva.",
    },
    {
        .title = "STIX/TAXII",
        .definition =
            "STIX modela inteligencia de amenazas; TAXII la transporta. Permiten compartir "
            "indicadores, malware, campanas, relaciones y cursos de accion.\n\n"
            "Uso: feeds, enriquecimiento, normalizacion y trazabilidad.",
    },
    {
        .title = "MITRE ATT&CK",
        .definition =
            "Base de conocimiento de tacticas y tecnicas adversarias. Organiza comportamiento "
            "por plataformas y fases operativas.\n\n"
            "Uso: mapear cobertura, construir detecciones, emular adversarios y reportar gaps.",
    },
    {
        .title = "Diamond Model",
        .definition =
            "Modelo analitico con cuatro vertices: adversary, capability, infrastructure y victim. "
            "Ayuda a relacionar eventos y formular hipotesis.\n\n"
            "Uso: clustering, pivoting y analisis de campanas.",
    },
    {
        .title = "Cyber Kill Chain",
        .definition =
            "Modelo de fases: reconnaissance, weaponization, delivery, exploitation, installation, "
            "C2 y actions on objectives.\n\n"
            "Uso: ubicar controles y detecciones por fase de ataque.",
    },
    {
        .title = "OSINT",
        .definition =
            "Open Source Intelligence obtiene informacion de fuentes publicas: dominios, leaks, "
            "repositorios, redes, metadatos, certificados y exposicion cloud.\n\n"
            "Practica: validar fuente, legalidad, fecha y confiabilidad.",
    },
    {
        .title = "Threat Actor",
        .definition =
            "Entidad o grupo con motivacion, recursos, TTP y objetivos. Puede ser criminal, estatal, "
            "hacktivista, insider o oportunista.\n\n"
            "Analisis: atribucion prudente, evidencia, confianza y sesgos.",
    },
    {
        .title = "Campaign",
        .definition =
            "Conjunto de actividades relacionadas por objetivos, infraestructura, malware, victimas "
            "o periodo. No implica atribucion definitiva.\n\n"
            "Uso: agrupar IoC, TTP, tiempos, sectores y respuestas.",
    },
    {
        .title = "Confidence Scoring",
        .definition =
            "Medida de certeza de una conclusion de inteligencia. Depende de fuente, corroboracion, "
            "actualidad y calidad de evidencia.\n\n"
            "Practica: separar hechos, inferencias y juicios analiticos.",
    },
};

static const WikiFlipTerm wikiflip_hardware_terms[] = {
    {
        .title = "UART",
        .definition =
            "Interfaz serial comun para consolas de debug. Normalmente expone TX, RX, GND y a veces "
            "VCC.\n\n"
            "Auditoria: identificar voltaje, baud rate, boot logs, prompts, credenciales y comandos "
            "privilegiados.",
    },
    {
        .title = "JTAG",
        .definition =
            "Interfaz de test/debug que puede permitir halt de CPU, memoria, firmware dumping y "
            "debug bajo nivel.\n\n"
            "Mitigacion: deshabilitar debug en produccion, proteger fuses y usar secure boot.",
    },
    {
        .title = "SWD",
        .definition =
            "Serial Wire Debug es debug ARM de dos hilos: SWDIO y SWCLK. Puede exponer flash, "
            "registros y control de ejecucion.\n\n"
            "Auditoria: verificar readout protection, mass erase y acceso fisico.",
    },
    {
        .title = "SPI Flash",
        .definition =
            "Memoria externa usada para firmware, configs o datos. Puede extraerse con clip o "
            "programador si no hay protecciones.\n\n"
            "Auditoria: dump, strings, binwalk, secretos, particiones y firmware signing.",
    },
    {
        .title = "I2C",
        .definition =
            "Bus de dos hilos para sensores, EEPROM y perifericos. Usa direcciones y lineas SDA/SCL.\n\n"
            "Auditoria: enumerar dispositivos, observar trafico, leer EEPROM y validar controles "
            "sobre datos sensibles.",
    },
    {
        .title = "Logic Analyzer",
        .definition =
            "Herramienta para capturar senales digitales y decodificar buses como UART, I2C, SPI "
            "o 1-Wire.\n\n"
            "Uso: identificar protocolos, timing, comandos y secretos transmitidos en claro.",
    },
    {
        .title = "RF Sniffing",
        .definition =
            "Captura de comunicaciones radio para analizar modulacion, frecuencia, tramas y "
            "repeticion.\n\n"
            "Auditoria: validar cifrado, rolling codes, replay resistance y exposicion de metadatos.",
    },
    {
        .title = "NFC/RFID",
        .definition =
            "Tecnologias de identificacion por radio. Incluyen LF RFID, HF NFC, MIFARE, DESFire y "
            "emulacion de tags.\n\n"
            "Auditoria: tipo de tag, claves, UID-only access, sectores, autenticacion y replay.",
    },
    {
        .title = "GPIO",
        .definition =
            "Pines de entrada/salida general usados para control, debug o boot straps.\n\n"
            "Auditoria: identificar pines, niveles logicos, estados de arranque, funciones ocultas "
            "y proteccion electrica.",
    },
    {
        .title = "Bootloader",
        .definition =
            "Codigo inicial que prepara hardware y carga firmware. Si queda expuesto puede permitir "
            "dump, flash o bypass.\n\n"
            "Auditoria: comandos, password, secure boot, rollback, recovery mode y firmas.",
    },
    {
        .title = "Secure Boot",
        .definition =
            "Cadena de arranque que verifica firmas antes de ejecutar firmware. Protege integridad "
            "si las claves y fuses estan bien gestionados.\n\n"
            "Auditoria: verificar enforcement, anti-rollback, key storage y modos debug.",
    },
    {
        .title = "Fault Injection",
        .definition =
            "Tecnica de inducir fallos fisicos con glitch de voltaje, clock, laser o EM para alterar "
            "flujo de ejecucion.\n\n"
            "Mitigacion: redundancia, sensores, timing checks, secure elements y boot robusto.",
    },
};

static const WikiFlipTerm wikiflip_iso_terms[] = {
    {
        .title = "ISO 27001",
        .definition =
            "Requisitos para un Sistema de Gestion de Seguridad de la Informacion. Centra alcance, "
            "riesgos, liderazgo, soporte, operacion, evaluacion y mejora.\n\n"
            "Auditoria: SoA, tratamiento de riesgos, evidencias, auditorias internas y acciones "
            "correctivas.",
    },
    {
        .title = "ISO 27002",
        .definition =
            "Guia de implementacion de controles de seguridad. Complementa ISO 27001 con medidas "
            "practicas y atributos.\n\n"
            "Auditoria: diseno, implementacion, madurez, brechas y alineacion con riesgo.",
    },
    {
        .title = "ISO 27005",
        .definition =
            "Guia para gestion de riesgos de seguridad de la informacion. Cubre contexto, analisis, "
            "evaluacion, tratamiento, comunicacion y monitoreo.\n\n"
            "Uso: construir metodologia repetible y trazable.",
    },
    {
        .title = "ISO 27017",
        .definition =
            "Controles de seguridad para servicios cloud, tanto proveedores como clientes.\n\n"
            "Auditoria: responsabilidad compartida, configuracion, virtualizacion, administracion "
            "y segregacion de clientes.",
    },
    {
        .title = "ISO 27018",
        .definition =
            "Proteccion de datos personales en cloud publica para procesadores de PII.\n\n"
            "Auditoria: consentimiento, uso limitado, disclosure, deletion, subprocesadores y "
            "transparencia.",
    },
    {
        .title = "ISO 22301",
        .definition =
            "Sistema de gestion de continuidad del negocio. Orienta BIA, estrategias, planes, "
            "ejercicios y mejora continua.\n\n"
            "Auditoria: RTO/RPO, pruebas, dependencias, crisis management y recuperacion.",
    },
    {
        .title = "ISO 27701",
        .definition =
            "Extension de privacidad para ISO 27001/27002. Define PIMS y controles para responsables "
            "y procesadores de datos personales.\n\n"
            "Auditoria: inventario PII, bases legales, derechos, DPIA y terceros.",
    },
    {
        .title = "ISO 27035",
        .definition =
            "Gestion de incidentes de seguridad. Cubre planificacion, deteccion, reporte, respuesta "
            "y aprendizaje.\n\n"
            "Auditoria: roles, severidad, canales, evidencias, lecciones y metricas.",
    },
    {
        .title = "ISO 27032",
        .definition =
            "Guia de ciberseguridad enfocada en colaboracion, intercambio de informacion y riesgos "
            "en el ciberespacio.\n\n"
            "Uso: alinear seguridad de informacion, redes, internet y proteccion critica.",
    },
    {
        .title = "ISO 42001",
        .definition =
            "Sistema de gestion para inteligencia artificial. Define gobierno, riesgos, ciclo de "
            "vida, transparencia, supervision y mejora.\n\n"
            "Auditoria: inventario de IA, controles, datos, evaluaciones, monitoreo y responsabilidades.",
    },
};

static const WikiFlipCategory wikiflip_categories[] = {
    {
        .name = "Protocolos",
        .terms = wikiflip_protocolos_terms,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_protocolos_terms),
    },
    {
        .name = "OWASP",
        .terms = wikiflip_owasp_terms,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_owasp_terms),
    },
    {
        .name = "NIST",
        .terms = wikiflip_nist_terms,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_nist_terms),
    },
    {
        .name = "CVE",
        .terms = wikiflip_cve_terms,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_cve_terms),
    },
    {
        .name = "BlueTeam",
        .terms = wikiflip_blueteam_terms,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_blueteam_terms),
    },
    {
        .name = "RedTeam",
        .terms = wikiflip_redteam_terms,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_redteam_terms),
    },
    {
        .name = "Threat Intelligence",
        .terms = wikiflip_threat_intelligence_terms,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_threat_intelligence_terms),
    },
    {
        .name = "HardwareHacking",
        .terms = wikiflip_hardware_terms,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_hardware_terms),
    },
    {
        .name = "ISO",
        .terms = wikiflip_iso_terms,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_iso_terms),
    },
};

static void wikiflip_term_selected_callback(void* context, uint32_t index);

static void wikiflip_switch_to_view(WikiFlipApp* app, WikiFlipView view) {
    app->current_view = view;
    view_dispatcher_switch_to_view(app->view_dispatcher, view);
}

static void wikiflip_show_definition(WikiFlipApp* app, const WikiFlipTerm* term) {
    widget_reset(app->definition_widget);
    furi_string_printf(app->definition_buffer, "%s\n\n%s", term->title, term->definition);
    widget_add_text_scroll_element(
        app->definition_widget,
        0,
        0,
        WIKIFLIP_SCREEN_WIDTH,
        WIKIFLIP_SCREEN_HEIGHT,
        furi_string_get_cstr(app->definition_buffer));
}

static void wikiflip_terms_menu_rebuild(WikiFlipApp* app) {
    furi_assert(app->current_category);

    submenu_reset(app->terms_menu);
    submenu_set_header(app->terms_menu, app->current_category->name);

    for(size_t i = 0; i < app->current_category->term_count; i++) {
        submenu_add_item(
            app->terms_menu,
            app->current_category->terms[i].title,
            (uint32_t)i,
            wikiflip_term_selected_callback,
            app);
    }
}

static void wikiflip_term_selected_callback(void* context, uint32_t index) {
    WikiFlipApp* app = context;
    furi_assert(app);
    furi_assert(app->current_category);

    if(index >= app->current_category->term_count) {
        return;
    }

    app->current_term_index = index;
    wikiflip_show_definition(app, &app->current_category->terms[index]);
    wikiflip_switch_to_view(app, WikiFlipViewDefinition);
}

static void wikiflip_category_selected_callback(void* context, uint32_t index) {
    WikiFlipApp* app = context;
    furi_assert(app);

    if(index >= WIKIFLIP_ARRAY_SIZE(wikiflip_categories)) {
        return;
    }

    app->current_category_index = index;
    app->current_term_index = 0;
    app->current_category = &wikiflip_categories[index];

    wikiflip_terms_menu_rebuild(app);
    submenu_set_selected_item(app->terms_menu, app->current_term_index);

    wikiflip_switch_to_view(app, WikiFlipViewTerms);
}

static bool wikiflip_navigation_callback(void* context) {
    WikiFlipApp* app = context;
    furi_assert(app);

    switch(app->current_view) {
    case WikiFlipViewDefinition:
        submenu_set_selected_item(app->terms_menu, app->current_term_index);
        wikiflip_switch_to_view(app, WikiFlipViewTerms);
        return true;
    case WikiFlipViewTerms:
        submenu_set_selected_item(app->categories_menu, app->current_category_index);
        wikiflip_switch_to_view(app, WikiFlipViewCategories);
        return true;
    case WikiFlipViewCategories:
    default:
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
}

static void wikiflip_categories_menu_build(WikiFlipApp* app) {
    submenu_reset(app->categories_menu);
    submenu_set_header(app->categories_menu, "WikiFlip");

    for(size_t i = 0; i < WIKIFLIP_ARRAY_SIZE(wikiflip_categories); i++) {
        submenu_add_item(
            app->categories_menu,
            wikiflip_categories[i].name,
            (uint32_t)i,
            wikiflip_category_selected_callback,
            app);
    }

    submenu_set_selected_item(app->categories_menu, app->current_category_index);
}

static WikiFlipApp* wikiflip_alloc(void) {
    WikiFlipApp* app = malloc(sizeof(WikiFlipApp));
    furi_assert(app);

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->categories_menu = submenu_alloc();
    app->terms_menu = submenu_alloc();
    app->definition_widget = widget_alloc();
    app->definition_buffer = furi_string_alloc();
    app->current_category = &wikiflip_categories[0];
    app->current_category_index = 0;
    app->current_term_index = 0;
    app->current_view = WikiFlipViewCategories;

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, wikiflip_navigation_callback);

    view_dispatcher_add_view(
        app->view_dispatcher, WikiFlipViewCategories, submenu_get_view(app->categories_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, WikiFlipViewTerms, submenu_get_view(app->terms_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, WikiFlipViewDefinition, widget_get_view(app->definition_widget));

    wikiflip_categories_menu_build(app);
    wikiflip_terms_menu_rebuild(app);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void wikiflip_free(WikiFlipApp* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, WikiFlipViewDefinition);
    view_dispatcher_remove_view(app->view_dispatcher, WikiFlipViewTerms);
    view_dispatcher_remove_view(app->view_dispatcher, WikiFlipViewCategories);

    widget_free(app->definition_widget);
    submenu_free(app->terms_menu);
    submenu_free(app->categories_menu);
    view_dispatcher_free(app->view_dispatcher);
    furi_string_free(app->definition_buffer);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t wikiflip_app(void* p) {
    UNUSED(p);

    WikiFlipApp* app = wikiflip_alloc();
    wikiflip_switch_to_view(app, WikiFlipViewCategories);
    view_dispatcher_run(app->view_dispatcher);
    wikiflip_free(app);

    return 0;
}
