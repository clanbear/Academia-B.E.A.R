// BEAR_SpawnEntry.c
// Entrada de la tabla UID → entidad destino

[BaseContainerProps()]
class BEAR_SpawnEntry
{
    [Attribute("", UIWidgets.EditBox, "Identity ID del jugador (UUID de Reforger)")]
    string uid;

    [Attribute("", UIWidgets.EditBox, "Nombre exacto de la entidad destino en el mundo")]
    string nombreEntidadDestino;
}