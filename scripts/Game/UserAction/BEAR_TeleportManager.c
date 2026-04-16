//------------------------------------------------------------------------------------------------
//! Gestor de comunicación de red para el sistema de teletransporte BEAR.
//!
//! FLUJO EN SERVIDOR DEDICADO:
//!   1. Cliente pulsa acción → PerformAction() en CLIENTE
//!   2. Cliente llama pc.BEAR_RPC_SolicitarTeleport() → RPC llega al SERVIDOR
//!   3. Servidor valida, mueve al jugador y llama Rpc(pc.BEAR_RPC_MostrarHint()) → RPC llega al CLIENTE
//!
//! INSTALACIÓN:
//!   Coloca este archivo junto a los otros scripts BEAR y compila con F7.
//!   No hay prefabs que modificar.

modded class SCR_PlayerController
{
	//! Timestamps del último teleport por jugador — solo se usa en servidor
	protected ref map<int, float> m_mBEAR_UltimoUso;

	//------------------------------------------------------------------------------------------------
	// ── RPC CLIENTE → SERVIDOR ──────────────────────────────────────────────
	//------------------------------------------------------------------------------------------------

	//! Petición de teletransporte a destino fijo. Se ejecuta en el servidor.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void BEAR_RPC_SolicitarTeleport(string nombreDestino, int cooldown, string nombreMostrar)
	{
		IEntity jugador = GetControlledEntity();
		if (!jugador)
			return;

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;

		int playerId = pm.GetPlayerIdFromControlledEntity(jugador);
		if (playerId <= 0)
			return;

		if (!m_mBEAR_UltimoUso)
			m_mBEAR_UltimoUso = new map<int, float>();

		// Comprobar cooldown
		float tiempoActual = GetGame().GetWorld().GetWorldTime();
		if (m_mBEAR_UltimoUso.Contains(playerId))
		{
			float transcurrido = (tiempoActual - m_mBEAR_UltimoUso.Get(playerId)) / 1000.0;
			float restante = cooldown - transcurrido;
			if (restante > 0)
			{
				Rpc(BEAR_RPC_MostrarHint, string.Format("Espera %1 segundos", Math.Ceil(restante)), "Viaje Rápido");
				return;
			}
		}

		// Buscar entidad destino
		IEntity destino = GetGame().GetWorld().FindEntityByName(nombreDestino);
		if (!destino)
		{
			Rpc(BEAR_RPC_MostrarHint, "Destino no encontrado.", "Viaje Rápido");
			return;
		}

		// Comprobar distancia mínima
		if (vector.Distance(jugador.GetOrigin(), destino.GetOrigin()) < 1.5)
		{
			Rpc(BEAR_RPC_MostrarHint, "Ya estás en el destino.", "Viaje Rápido");
			return;
		}

		// Calcular posición final con offset y mover
		vector dir = (jugador.GetOrigin() - destino.GetOrigin()).Normalized();
		jugador.SetOrigin(destino.GetOrigin() + dir * 1.0);

		Physics fisica = jugador.GetPhysics();
		if (fisica)
			fisica.SetVelocity(vector.Zero);

		m_mBEAR_UltimoUso.Set(playerId, tiempoActual);

		Rpc(BEAR_RPC_MostrarHint, string.Format("Transportando a: %1", nombreMostrar), "Viaje Rápido");
	}

	//! Petición de teletransporte aleatorio. Se ejecuta en el servidor.
	//! destinosSerializados: nombres de entidades separados por "|"
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void BEAR_RPC_SolicitarTeleportAleatorio(string destinosSerializados, int cooldown)
	{
		IEntity jugador = GetControlledEntity();
		if (!jugador)
			return;

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;

		int playerId = pm.GetPlayerIdFromControlledEntity(jugador);
		if (playerId <= 0)
			return;

		if (!m_mBEAR_UltimoUso)
			m_mBEAR_UltimoUso = new map<int, float>();

		// Comprobar cooldown
		float tiempoActual = GetGame().GetWorld().GetWorldTime();
		if (m_mBEAR_UltimoUso.Contains(playerId))
		{
			float transcurrido = (tiempoActual - m_mBEAR_UltimoUso.Get(playerId)) / 1000.0;
			float restante = cooldown - transcurrido;
			if (restante > 0)
			{
				Rpc(BEAR_RPC_MostrarHint, string.Format("Espera %1 segundos", Math.Ceil(restante)), "Viaje Rápido");
				return;
			}
		}

		// Deserializar lista (separador "|")
		array<string> lista = new array<string>();
		destinosSerializados.Split("|", lista, true);

		// Filtrar destinos válidos
		array<string> validos = new array<string>();
		foreach (string nombre : lista)
		{
			if (nombre.IsEmpty())
				continue;
			IEntity ent = GetGame().GetWorld().FindEntityByName(nombre);
			if (!ent)
				continue;
			if (vector.Distance(jugador.GetOrigin(), ent.GetOrigin()) < 1.5)
				continue;
			validos.Insert(nombre);
		}

		if (validos.IsEmpty())
		{
			Rpc(BEAR_RPC_MostrarHint, "No hay destinos válidos disponibles.", "Viaje Rápido");
			return;
		}

		// Elegir destino aleatorio y mover
		string elegido = validos[Math.RandomInt(0, validos.Count())];
		IEntity destino = GetGame().GetWorld().FindEntityByName(elegido);
		if (!destino)
		{
			Rpc(BEAR_RPC_MostrarHint, "Error al localizar el destino.", "Viaje Rápido");
			return;
		}

		vector dir = (jugador.GetOrigin() - destino.GetOrigin()).Normalized();
		jugador.SetOrigin(destino.GetOrigin() + dir * 1.0);

		Physics fisica = jugador.GetPhysics();
		if (fisica)
			fisica.SetVelocity(vector.Zero);

		m_mBEAR_UltimoUso.Set(playerId, tiempoActual);

		Rpc(BEAR_RPC_MostrarHint, "Teletransporte completado.", "Viaje Rápido");
	}

	//------------------------------------------------------------------------------------------------
	// ── RPC SERVIDOR → CLIENTE ──────────────────────────────────────────────
	//------------------------------------------------------------------------------------------------

	//! Muestra un hint en el cliente propietario de este PlayerController.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void BEAR_RPC_MostrarHint(string mensaje, string titulo)
	{
		SCR_HintManagerComponent.ShowCustomHint(mensaje, titulo, 3.0);
	}
	
	//------------------------------------------------------------------------------------------------
	// ── SPAWN REDIRECTOR ────────────────────────────────────────────────────
	//------------------------------------------------------------------------------------------------

	//! Llamado desde el servidor (BEAR_SpawnRedirector) para teletransportar
	//! al jugador a su zona asignada tras el spawn.
	//! Se ejecuta EN EL SERVIDOR gracias a RplRcver.Server.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void BEAR_RPC_EjecutarSpawnRedirect(string nombreDestino)
	{
		IEntity jugador = GetControlledEntity();
		if (!jugador)
			return;

		IEntity destino = GetGame().GetWorld().FindEntityByName(nombreDestino);
		if (!destino)
		{
			Print(string.Format("[BEAR_TeleportManager] ERROR: entidad destino '%1' no encontrada.",
				nombreDestino), LogLevel.ERROR);
			Rpc(BEAR_RPC_MostrarHint,
				string.Format("Error: zona '%1' no encontrada.", nombreDestino),
				"Academia BEAR");
			return;
		}

		vector posDestino = destino.GetOrigin();
		posDestino[1] = posDestino[1] + 0.1;
		jugador.SetOrigin(posDestino);

		Physics fisica = jugador.GetPhysics();
		if (fisica)
			fisica.SetVelocity(vector.Zero);

		Rpc(BEAR_RPC_MostrarHint,
			string.Format("Bienvenido a tu zona: %1", nombreDestino),
			"Academia BEAR");
	}
}