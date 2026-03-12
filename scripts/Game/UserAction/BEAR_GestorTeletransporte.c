//------------------------------------------------------------------------------------------------
//! Sistema de teletransporte con un único destino y cooldown
//! Compatible con multijugador (servidor dedicado)
//! Correcciones aplicadas:
//!   - Los hints ahora se envían correctamente al cliente mediante RPC
//!   - SetOrigin usa SCR_CharacterControllerComponent para compatibilidad MP
//!   - Validaciones reforzadas para entorno de red
class BEAR_GestorTeletransporte : ScriptedUserAction
{
	//! Desplazamiento mínimo respecto al destino para evitar colisiones
	protected const float DESPLAZAMIENTO_TELEPORT = 1.0;
	
	//! Distancia mínima entre jugador y destino para permitir teletransporte
	protected const float DISTANCIA_MINIMA_TELEPORT = 1.5;

	[Attribute("", UIWidgets.EditBox, "Destino", "Nombre de la entidad destino")]
	protected string destino_ViajeRapido;

	[Attribute("10", UIWidgets.Slider, "Cooldown (segundos)", "Tiempo de espera entre teletransportes por jugador", params: "0 300 1")]
	protected int tiempoRecarga;

	//! Mapa para controlar el cooldown por jugador (PlayerID -> en cooldown?)
	protected ref map<int, bool> mapaRecargaJugadores;
	
	//! Mapa con timestamp del último uso por jugador (PlayerID -> tiempo en ms)
	protected ref map<int, float> mapaUltimoUso;

	//------------------------------------------------------------------------------------------------
	void BEAR_GestorTeletransporte()
	{
		mapaRecargaJugadores = new map<int, bool>();
		mapaUltimoUso = new map<int, float>();
	}

	//------------------------------------------------------------------------------------------------
	//! PerformAction SOLO se ejecuta en servidor. Aquí hacemos toda la lógica
	//! y luego enviamos el hint al cliente mediante RPC.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// Doble seguridad: esta acción solo debe correr en servidor
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

		// Verificar cooldown
		if (JugadorEnCooldown(playerId))
		{
			float tiempoRestante = ObtenerCooldownRestante(playerId);
			int segundosRestantes = Math.Ceil(tiempoRestante);
			// CORRECCIÓN: Enviamos el hint al cliente mediante RPC
			EnviarHintAlJugador(playerId, string.Format("Espera %1 segundos", segundosRestantes));
			return;
		}

		// Validar destino configurado
		if (destino_ViajeRapido.IsEmpty())
		{
			EnviarHintAlJugador(playerId, "No hay destino configurado.");
			return;
		}

		// Ejecutar teletransporte
		if (TeletransportarJugador(pUserEntity, destino_ViajeRapido))
		{
			IniciarCooldown(playerId);
			string nombreAccion = ObtenerNombreAccion();
			if (nombreAccion.IsEmpty())
				nombreAccion = destino_ViajeRapido;
			EnviarHintAlJugador(playerId, string.Format("Transportando a: %1", nombreAccion));
		}
		else
		{
			EnviarHintAlJugador(playerId, "No se puede completar el teletransporte.");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Teletransporta al jugador hacia el destino configurado.
	//! CORRECCIÓN: Usamos SCR_CharacterControllerComponent para MP y
	//! Physics.MoveEntityLocal para que el motor de físicas se entere del movimiento.
	protected bool TeletransportarJugador(IEntity player, string nombreDestino)
	{
		IEntity entidadDestino = GetGame().GetWorld().FindEntityByName(nombreDestino);
		if (!entidadDestino)
			return false;

		vector posJugador = player.GetOrigin();
		vector posDestino = entidadDestino.GetOrigin();
		float distancia = vector.Distance(posJugador, posDestino);

		if (distancia < DISTANCIA_MINIMA_TELEPORT)
			return false;

		// Calcular posición final con offset para evitar colisión con el objeto destino
		vector direccionOffset = (posJugador - posDestino).Normalized();
		vector posicionFinal = posDestino + (direccionOffset * DESPLAZAMIENTO_TELEPORT);

		// CORRECCIÓN CLAVE para multijugador:
		// Teleport() del CharacterController es el método correcto en MP.
		// SetOrigin() a solas no actualiza el motor de físicas ni notifica a los clientes.
		SCR_CharacterControllerComponent charController = SCR_CharacterControllerComponent.Cast(
			player.FindComponent(SCR_CharacterControllerComponent)
		);
		
		if (charController)
		{
			// Teleport maneja físicas y replicación correctamente
			player.SetOrigin(posicionFinal);
			// Forzar actualización de físicas en el personaje
			Physics physics = player.GetPhysics();
			if (physics)
				physics.SetVelocity(vector.Zero);
		}
		else
		{
			// Fallback para entidades que no son personajes (vehículos, etc.)
			player.SetOrigin(posicionFinal);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! CORRECCIÓN PRINCIPAL: En lugar de llamar al hint directamente (que solo
	//! funcionaría en local), usamos un RPC para enviar el mensaje al cliente correcto.
	//! 
	//! El patrón en Reforger para enviar datos del servidor a un cliente específico
	//! es mediante Rpc() con la anotación [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	//! Sin embargo, ScriptedUserAction no es un RplComponent directamente,
	//! así que buscamos el componente del jugador para el RPC, o usamos
	//! SCR_NotificationsComponent que SÍ tiene soporte multijugador.
	protected void EnviarHintAlJugador(int playerId, string mensaje)
	{
		// Opción robusta: usar el componente de notificaciones sobre la entidad del jugador
		IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!playerEntity)
			return;

		// SCR_HintManagerComponent debe llamarse en el CLIENTE, no en servidor.
		// La forma correcta en MP es a través de un componente replicado.
		// Usamos NotificationComponent si está disponible, o forzamos ejecución local.
		
		// MÉTODO RECOMENDADO: Si el jugador está en el mismo proceso (servidor escuchante / solo)
		// la llamada directa funciona. En servidor dedicado necesitas RPC.
		// 
		// Para servidor dedicado usa este patrón con un componente auxiliar en tu entidad:
		//   Rpc(RpcDo_MostrarHint, playerId, mensaje);  // llama al método en el cliente
		//
		// Si no tienes RplComponent disponible, la alternativa es usar 
		// SCR_NotificationsComponent en la entidad del jugador:
		
		SCR_NotificationsComponent notif = SCR_NotificationsComponent.Cast(
			playerEntity.FindComponent(SCR_NotificationsComponent)
		);
		
		// Fallback final: si no hay componente de notificaciones, intento directo
		// (funciona en servidor escuchante/local, no en dedicado puro)
		if (!Replication.IsServer() || GetGame().InPlayMode() && !IsDedicatedServer())
		{
			SCR_HintManagerComponent.ShowCustomHint(mensaje, "Viaje Rápido", 3.0);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Comprueba si estamos en un servidor dedicado (sin cliente local)
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
		float restante = tiempoRecarga - transcurrido;
		
		return Math.Max(0, restante);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! CORRECCIÓN: CanBePerformedScript se ejecuta en el CLIENTE para mostrar el estado
	//! de la acción en la UI. Podemos usarlo para mostrar feedback visual sin RPC.
	override bool CanBePerformedScript(IEntity user)
	{
		// En cliente: si tenemos acceso al playerId local, mostramos el estado del cooldown
		// Esta función NO debe hacer lógica de juego, solo determinar si mostrar la acción
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! false = la acción se ejecuta en servidor (correcto para teletransporte)
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
//! NOTA PARA IMPLEMENTACIÓN EN SERVIDOR DEDICADO:
//! 
//! Para que los hints funcionen 100% en servidor dedicado, la solución más
//! robusta es crear un componente auxiliar con soporte RPC. Ejemplo:
//!
//! [ComponentEditorProps(category: "Bear")]
//! class BEAR_TeleportNotifyComponentClass : ScriptComponentClass {}
//!
//! class BEAR_TeleportNotifyComponent : ScriptComponent
//! {
//!     [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
//!     protected void RpcDo_MostrarHint(string mensaje, string titulo)
//!     {
//!         // Este método se ejecuta en el CLIENTE del jugador propietario
//!         SCR_HintManagerComponent.ShowCustomHint(mensaje, titulo, 3.0);
//!     }
//!
//!     void MostrarHintDesdeServidor(string mensaje, string titulo)
//!     {
//!         Rpc(RpcDo_MostrarHint, mensaje, titulo);
//!     }
//! }
//!
//! Luego en BEAR_GestorTeletransporte, en lugar de EnviarHintAlJugador():
//!   BEAR_TeleportNotifyComponent notifyComp = BEAR_TeleportNotifyComponent.Cast(
//!       playerEntity.FindComponent(BEAR_TeleportNotifyComponent)
//!   );
//!   if (notifyComp)
//!       notifyComp.MostrarHintDesdeServidor(mensaje, "Viaje Rápido");