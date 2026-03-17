//------------------------------------------------------------------------------------------------
//! Entrada de la tabla UID → zona de spawn.
//! Cada entrada asocia el UID de Steam de un jugador con el nombre exacto
//! de la entidad del mundo que se usará como punto de spawn.
//!
//! Se edita directamente en el panel de atributos del componente BEAR_SpawnManager
//! dentro del Workbench.

[BaseContainerProps()]
class BEAR_SpawnEntry
{
	[Attribute("", UIWidgets.EditBox, "Identity ID del jugador (UUID, ej: 7a08923b-1e5f-8e67-bf43-9g5d4331cc68)")]
	string uid;

	[Attribute("", UIWidgets.EditBox, "Nombre exacto de la entidad destino en el mundo")]
	string nombreEntidadDestino;
}