//------------------------------------------------------------------------------------------------
//! Sistema de spawn personalizado por UID para misiones de academia.
//!
//! Asigna a cada jugador una zona de spawn propia (indicada por el nombre de una entidad
//! en el mundo, igual que el sistema de teleports BEAR) que se usa tanto en la conexión
//! inicial como en cada respawn.
//!
//! ── FLUJO ──────────────────────────────────────────────────────────────────
//!   Al conectarse o morir un jugador:
//!     1. SCR_BaseGameMode dispara OnPlayerSpawned() → en SERVIDOR
//!     2. Buscamos su UID en la tabla m_aEntradas
//!     3. Si hay entrada, localizamos la entidad destino en el mundo
//!     4. Teleportamos al jugador allí (misma lógica que BEAR_TeleportManager)
//!
//! ── INSTALACIÓN ─────────────────────────────────────────────────────────────
//!   1. Copia este archivo y BEAR_SpawnEntry.c en la misma carpeta que los otros scripts BEAR.
//!      Ejemplo: Scripts/Game/Bear/
//!   2. Compila con F7 en el Workbench (Script Editor).
//!   3. Abre el prefab de tu GameMode (o el objeto GameMode en el editor de misión).
//!   4. Añade el componente "BEAR_SpawnManager" (botón "+" en el panel de componentes).
//!   5. Rellena la lista "Entradas UID → Zona":
//!        · uid              → UID de Steam exacto del jugador (string, p.ej. "76561198012345678")
//!        · nombreEntidadDestino → nombre exacto de la entidad en el mundo (igual que en los teleports)
//!   6. Guarda y lanza la misión.
//!
//! ── NOTAS ───────────────────────────────────────────────────────────────────
//!   · Si un jugador no tiene entrada en la tabla, el spawn normal de la misión no se altera.
//!   · El componente solo actúa en el servidor; en clientes no hace nada.
//!   · Requiere BEAR_TeleportManager.c en el proyecto solo si quieres usar sus RPCs de hint;
//!     en caso contrario este archivo es completamente independiente.

[ComponentEditorProps(category: "Bear")]
class BEAR_SpawnManagerClass : ScriptComponentClass {}

class BEAR_SpawnManager : ScriptComponent
{
	//! Tabla de asignaciones UID → entidad destino.
	//! Se rellena en el editor de Workbench.
	[Attribute("", UIWidgets.Auto, "Entradas UID → Zona", "",
		params: "BEAR_SpawnEntry")]
	protected ref array<ref BEAR_SpawnEntry> m_aEntradas;

	//! Caché interna para búsquedas rápidas: uid → nombreEntidadDestino
	protected ref map<string, string> m_mCacheUID;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		// Solo el servidor gestiona spawns
		if (!Replication.IsServer())
			return;

		// Construir caché
		m_mCacheUID = new map<string, string>();
		if (m_aEntradas)
		{
			foreach (BEAR_SpawnEntry entrada : m_aEntradas)
			{
				if (!entrada || entrada.uid.IsEmpty() || entrada.nombreEntidadDestino.IsEmpty())
					continue;
				m_mCacheUID.Set(entrada.uid, entrada.nombreEntidadDestino);
			}
		}

		// Suscribirse al evento de spawn del GameMode
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode)
			gameMode.GetOnPlayerSpawned().Insert(OnJugadorSpawned);
	}

	//------------------------------------------------------------------------------------------------
	//! Se dispara en el SERVIDOR cada vez que un jugador hace spawn (inicial o respawn).
	protected void OnJugadorSpawned(int playerId, IEntity jugador)
	{
		if (!jugador || !m_mCacheUID)
			return;

		// Obtener el Identity ID del jugador (UUID formato 7d08223b-1e5e-...)
		// Nota: es GetPlayerIdentityId con 'd' minúscula al final
		string uid = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
		if (uid.IsEmpty())
			return;

		// ¿Tiene zona asignada?
		if (!m_mCacheUID.Contains(uid))
			return;

		string nombreDestino = m_mCacheUID.Get(uid);
		if (nombreDestino.IsEmpty())
			return;

		// Localizar entidad destino
		IEntity destino = GetGame().GetWorld().FindEntityByName(nombreDestino);
		if (!destino)
		{
			Print(string.Format("[BEAR_SpawnManager] AVISO: entidad destino '%1' no encontrada para UID %2",
				nombreDestino, uid), LogLevel.WARNING);
			return;
		}

		// Pequeño offset para no quedar clavado dentro del objeto destino
		// (misma lógica que BEAR_TeleportManager, pero aquí el jugador acaba de spawnear
		//  en la posición por defecto, así que calculamos el offset desde el destino)
		vector posDestino = destino.GetOrigin();

		// Subimos ligeramente en Y para evitar que aparezca bajo el suelo
		posDestino[1] = posDestino[1] + 0.1;

		jugador.SetOrigin(posDestino);

		// Detener física residual
		Physics fisica = jugador.GetPhysics();
		if (fisica)
			fisica.SetVelocity(vector.Zero);

		// Hint de bienvenida al propio jugador via PlayerController modded
		// (requiere BEAR_TeleportManager.c; si no lo tienes, comenta estas líneas)
		SCR_PlayerController pc = SCR_PlayerController.Cast(
			GetGame().GetPlayerManager().GetPlayerController(playerId)
		);
		if (pc)
			pc.BEAR_RPC_MostrarHint(
				string.Format("Bienvenido a tu zona: %1", nombreDestino),
				"Academia BEAR"
			);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		// Desuscribirse para evitar referencias colgantes
		if (Replication.IsServer())
		{
			SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
			if (gameMode)
				gameMode.GetOnPlayerSpawned().Remove(OnJugadorSpawned);
		}

		super.OnDelete(owner);
	}
}