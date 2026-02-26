# Demo 04: Sandbag (fixed at one end)

A single deformable beam fixed at one end so you can see gravity-induced deformation clearly.

## Scene

- **SOFA scene:** `Content/SofaScenes/sandbag.scn`
- **Mesh:** `Content/SofaScenes/mesh/sandbag.msh` (simple tetrahedral box)

The left face (nodes 1, 4, 5, 8) is fixed; the rest deforms under gravity. A plane floor is added so the object doesn’t fall indefinitely.

## How to run

1. Create or open a level (e.g. duplicate **Demo_01_SimpleLiver** and rename).
2. Place a **SofaContext** actor (or use the existing one).
3. Set **File Path** to the sandbag scene:
   - Relative: `Content/SofaScenes/sandbag.scn`
   - Or use the full path to `SofaScenes/sandbag.scn` inside the plugin.
4. Press **Play**. One visual mesh (the sandbag) should spawn and sag under gravity from the fixed end.

## Parameters (in sandbag.scn)

- **FixedConstraint** on indices `1 4 5 8` (left face of the beam).
- **TetrahedronFEMForceField** with `youngModulus="800"`, `poissonRatio="0.3"`.
- **PlaneForceField** as floor; adjust `d` if you change scale or position.
- Scale `50 50 50` so the beam is visible; adjust in the scene file if needed.
