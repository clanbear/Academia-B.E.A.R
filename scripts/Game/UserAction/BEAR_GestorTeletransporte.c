//------------------------------------------------------------------------------------------------
//! Sistema de teletransporte con un único destino y cooldown
//! Configurable vía atributos del editor
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
	//! Constructor - inicializa el sistema de cooldown
	void BEAR_GestorTeletransporte()
	{
		mapaRecargaJugadores = new map<int, bool>();
		mapaUltimoUso = new map<int, float>();
	}

	//------------------------------------------------------------------------------------------------
	//! Ejecuta la acción de teletransporte con validaciones y cooldown
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (!ValidarEntidades(pOwnerEntity, pUserEntity))
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);

		// Verificar cooldown del jugador
		if (JugadorEnCooldown(playerId))
		{
			float tiempoRestante = ObtenerCooldownRestante(playerId);
			int segundosRestantes = Math.Ceil(tiempoRestante);
			MostrarSugerenciaAJugador(pUserEntity, string.Format("Espera %1 segundos", segundosRestantes));
			return;
		}

		// Validar destino configurado
		if (destino_ViajeRapido.IsEmpty())
		{
			MostrarSugerenciaAJugador(pUserEntity, "No hay destino configurado.");
			return;
		}

		// Ejecutar teletransporte
		string nombreOrigen = pOwnerEntity.GetName();
		if (TeletransportarJugador(pUserEntity, destino_ViajeRapido, nombreOrigen))
		{
			IniciarCooldown(playerId);
			MostrarSugerenciaAJugador(pUserEntity, string.Format("Transportando a: %1", ObtenerNombreAccion()));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool ValidarEntidades(IEntity owner, IEntity user)
	{
		return owner && user;
	}

	//------------------------------------------------------------------------------------------------
	protected bool JugadorEnCooldown(int playerId)
	{
		return mapaRecargaJugadores.Contains(playerId) && mapaRecargaJugadores.Get(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected bool TeletransportarJugador(IEntity player, string nombreDestino, string nombreOrigen)
	{
		IEntity entidadDestino = GetGame().GetWorld().FindEntityByName(nombreDestino);

		if (!entidadDestino)
			return false;

		vector posJugador = player.GetOrigin();
		vector posDestino = entidadDestino.GetOrigin();
		float distancia = vector.Distance(posJugador, posDestino);

		// Verificar distancia mínima
		if (distancia < DISTANCIA_MINIMA_TELEPORT)
			return false;

		// Calcular posición final con offset
		vector direccionOffset = (posJugador - posDestino).Normalized();
		vector posicionFinal = posDestino + (direccionOffset * DESPLAZAMIENTO_TELEPORT);

		player.SetOrigin(posicionFinal);
		return true;
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
		
		if (restante < 0)
			return 0;
			
		return restante;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void MostrarSugerenciaAJugador(IEntity player, string mensaje)
	{
		if (!player)
			return;
			
		SCR_HintManagerComponent.ShowCustomHint(mensaje, "Viaje Rápido", 3.0);
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
	
	// Obtiene el nombre configurado en UI Info
	protected string ObtenerNombreAccion()
	{
	    UIInfo uiInfo = GetUIInfo();
	    if (!uiInfo)
	        return string.Empty;
	        
	    return uiInfo.GetName();
	}
}
