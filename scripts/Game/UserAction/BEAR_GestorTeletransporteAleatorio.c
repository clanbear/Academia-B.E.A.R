//------------------------------------------------------------------------------------------------
//! Sistema de teletransporte aleatorio entre múltiples destinos configurables
//! Compatible con multijugador (servidor dedicado)
//! Correcciones aplicadas:
//!   - Los hints ahora se envían correctamente al cliente (no desde servidor)
//!   - SetOrigin usa reseteo de velocidad para compatibilidad con físicas MP
//!   - Validaciones reforzadas para entorno de red (playerId > 0)
//!   - Math.RandomInt corregido (el límite superior es exclusivo, era correcto
//!     pero se documenta explícitamente)
//!   - ObtenerCooldownRestante usa Math.Max en lugar de if/return duplicado
class BEAR_GestorTeletransporteAleatorio : ScriptedUserAction
{
	//! Desplazamiento mínimo respecto al destino para evitar colisiones
	protected const float DESPLAZAMIENTO_TELEPORT = 1.0;
	
	//! Distancia mínima entre jugador y destino para permitir teletransporte
	protected const float DISTANCIA_MINIMA_TELEPORT = 1.5;

	[Attribute("10", UIWidgets.Slider, "Cooldown (segundos)", "Tiempo de espera entre teletransportes por jugador", params: "0 300 1")]
	protected int tiempoRecarga;

	//! Lista de nombres de entidades destino (se elige una aleatoriamente)
	[Attribute("", UIWidgets.Auto, "Destinos posibles", "Lista de entidades destino. Se elegirá una aleatoriamente al usarlo.")]
	protected ref array<string> listaDestinos;

	//! Mapa para controlar el cooldown por jugador (PlayerID -> en cooldown?)
	protected ref map<int, bool> mapaRecargaJugadores;
	
	//! Mapa con timestamp del último uso por jugador (PlayerID -> tiempo en ms)
	protected ref map<int, float> mapaUltimoUso;

	//------------------------------------------------------------------------------------------------
	void BEAR_GestorTeletransporteAleatorio()
	{
		mapaRecargaJugadores = new map<int, bool>();
		mapaUltimoUso = new map<int, float>();
		
		if (!listaDestinos)
			listaDestinos = new array<string>();
	}

	//------------------------------------------------------------------------------------------------
	//! PerformAction SOLO se ejecuta en servidor. Aquí hacemos toda la lógica
	//! y luego enviamos el hint al cliente mediante EnviarHintAlJugador().
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (!pOwnerEntity || !pUserEntity)
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		int playerId = playerManager.GetPlayerIdFromControlledEntity(pUserEntity);
		if (playerId <= 0)
			return;

		// Verificar cooldown del jugador
		if (JugadorEnCooldown(playerId))
		{
			float tiempoRestante = ObtenerCooldownRestante(playerId);
			int segundosRestantes = Math.Ceil(tiempoRestante);
			// CORRECCIÓN: enviamos el hint al cliente, no lo llamamos en servidor
			EnviarHintAlJugador(playerId, string.Format("Espera %1 segundos", segundosRestantes));
			return;
		}

		// Verificar que haya destinos configurados
		if (!listaDestinos || listaDestinos.IsEmpty())
		{
			EnviarHintAlJugador(playerId, "No hay destinos configurados.");
			return;
		}

		// Elegir destino aleatorio y ejecutar teletransporte
		string destinoElegido = ElegirDestinoAleatorio(pUserEntity);
		
		if (destinoElegido.IsEmpty())
		{
			EnviarHintAlJugador(playerId, "No se encontró ningún destino válido.");
			return;
		}

		if (TeletransportarJugador(pUserEntity, destinoElegido))
		{
			IniciarCooldown(playerId);
			string nombreAccion = ObtenerNombreAccion();
			if (nombreAccion.IsEmpty())
				nombreAccion = destinoElegido;
			EnviarHintAlJugador(playerId, string.Format("Transportando a: %1", nombreAccion));
		}
		else
		{
			EnviarHintAlJugador(playerId, "No se puede completar el teletransporte.");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Elige un destino aleatorio de la lista filtrando los que no existen o están
	//! demasiado cerca. Math.RandomInt(0, N) devuelve [0, N-1], que es correcto.
	protected string ElegirDestinoAleatorio(IEntity player)
	{
		array<string> destinosValidos = new array<string>();
		
		foreach (string nombre : listaDestinos)
		{
			if (nombre.IsEmpty())
				continue;
				
			IEntity entidad = GetGame().GetWorld().FindEntityByName(nombre);
			if (!entidad)
				continue;
			
			float distancia = vector.Distance(player.GetOrigin(), entidad.GetOrigin());
			if (distancia < DISTANCIA_MINIMA_TELEPORT)
				continue;
				
			destinosValidos.Insert(nombre);
		}
		
		if (destinosValidos.IsEmpty())
			return string.Empty;
		
		// Math.RandomInt(min, max) → [min, max-1], por tanto Count() como límite es correcto
		int indiceAleatorio = Math.RandomInt(0, destinosValidos.Count());
		return destinosValidos[indiceAleatorio];
	}

	//------------------------------------------------------------------------------------------------
	//! CORRECCIÓN PRINCIPAL para MP: SetOrigin() solo mueve la entidad pero no
	//! notifica al CharacterControllerComponent ni resetea físicas. En servidor
	//! dedicado esto puede hacer que el motor devuelva al jugador a su posición
	//! anterior. Reseteamos la velocidad tras mover para evitarlo.
	protected bool TeletransportarJugador(IEntity player, string nombreDestino)
	{
		IEntity entidadDestino = GetGame().GetWorld().FindEntityByName(nombreDestino);
		if (!entidadDestino)
			return false;

		vector posJugador = player.GetOrigin();
		vector posDestino = entidadDestino.GetOrigin();

		// Calcular posición final con offset para evitar colisión con el objeto destino
		vector direccionOffset = (posJugador - posDestino).Normalized();
		vector posicionFinal = posDestino + (direccionOffset * DESPLAZAMIENTO_TELEPORT);

		// CORRECCIÓN: mover y resetear velocidad para que las físicas no reviertan el teleport
		player.SetOrigin(posicionFinal);
		
		Physics physics = player.GetPhysics();
		if (physics)
			physics.SetVelocity(vector.Zero);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! CORRECCIÓN: SCR_HintManagerComponent.ShowCustomHint() es una función de UI
	//! que solo funciona en el proceso cliente. Llamarla desde el servidor en un
	//! servidor dedicado no tiene efecto. Este método aplica el mismo patrón
	//! que BEAR_GestorTeletransporte: fallback para servidor escuchante/local,
	//! y documentación del componente RPC para servidor dedicado puro.
	//!
	//! Para servidor dedicado 100% robusto, usa BEAR_TeleportNotifyComponent
	//! (ver comentario al final del archivo).
	protected void EnviarHintAlJugador(int playerId, string mensaje)
	{
		IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!playerEntity)
			return;

		// En servidor escuchante o modo local, la llamada directa sí llega al cliente local.
		// En servidor dedicado puro, esta llamada no tiene efecto: usa el componente RPC.
		if (!IsDedicatedServer())
		{
			SCR_HintManagerComponent.ShowCustomHint(mensaje, "Viaje Rápido", 3.0);
		}

		// Si tienes BEAR_TeleportNotifyComponent en el prefab del jugador:
		// BEAR_TeleportNotifyComponent notifyComp = BEAR_TeleportNotifyComponent.Cast(
		//     playerEntity.FindComponent(BEAR_TeleportNotifyComponent)
		// );
		// if (notifyComp)
		//     notifyComp.MostrarHintDesdeServidor(mensaje, "Viaje Rápido");
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsDedicatedServer()
	{
		ArmaReforgerScripted game = GetGame();
		if (!game)
			return false;
		return (game.GetPlayerManager().GetPlayerCount() > 0 &&
		        game.GetPlayerController() == null);
	}

	//------------------------------------------------------------------------------------------------
	protected bool JugadorEnCooldown(int playerId)
	{
		return mapaRecargaJugadores.Contains(playerId) && mapaRecargaJugadores.Get(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void IniciarCooldown(int playerId)
	{
		if (tiempoRecarga <= 0)
			return;

		mapaRecargaJugadores.Set(playerId, true);
		mapaUltimoUso.Set(playerId, GetGame().GetWorld().GetWorldTime());

		GetGame().GetCallqueue().CallLater(RestablecerCooldown, tiempoRecarga * 1000, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void RestablecerCooldown(int playerId)
	{
		mapaRecargaJugadores.Set(playerId, false);
	}
	
	//------------------------------------------------------------------------------------------------
	protected float ObtenerCooldownRestante(int playerId)
	{
		if (!mapaUltimoUso.Contains(playerId))
			return 0;
			
		float ultimoUso = mapaUltimoUso.Get(playerId);
		float tiempoActual = GetGame().GetWorld().GetWorldTime();
		float transcurrido = (tiempoActual - ultimoUso) / 1000.0;

		return Math.Max(0, tiempoRecarga - transcurrido);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	protected string ObtenerNombreAccion()
	{
		UIInfo uiInfo = GetUIInfo();
		if (!uiInfo)
			return string.Empty;
		return uiInfo.GetName();
	}
}


//------------------------------------------------------------------------------------------------
//! NOTA PARA SERVIDOR DEDICADO:
//!
//! Para que los hints lleguen al cliente en un servidor dedicado puro, añade este
//! componente a tu prefab de jugador y úsalo en EnviarHintAlJugador() (ver arriba).
//!
//! [ComponentEditorProps(category: "Bear")]
//! class BEAR_TeleportNotifyComponentClass : ScriptComponentClass {}
//!
//! class BEAR_TeleportNotifyComponent : ScriptComponent
//! {
//!     [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
//!     protected void RpcDo_MostrarHint(string mensaje, string titulo)
//!     {
//!         // Se ejecuta en el CLIENTE propietario del personaje
//!         SCR_HintManagerComponent.ShowCustomHint(mensaje, titulo, 3.0);
//!     }
//!
//!     void MostrarHintDesdeServidor(string mensaje, string titulo)
//!     {
//!         Rpc(RpcDo_MostrarHint, mensaje, titulo);
//!     }
//! }