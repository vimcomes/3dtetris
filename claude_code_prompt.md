# Claude Code Prompt — 3D Tetris Glassmorphism Visual Style

Use this prompt to implement the glassmorphism block rendering style in the original C++ 3D Tetris project.

---

Implement a glassmorphism visual style for the 3D Tetris blocks with the following properties:

## BLOCK GEOMETRY
- Each block is a cube with size 0.88×0.88×0.88 (slight gap between adjacent blocks)
- Every block has two layers: a translucent face mesh + bright edge wireframe on top

## FACE MATERIAL (MeshPhysicalMaterial / PBR)
- Base color: the piece's assigned color
- Emissive color: same as base color, intensity 0.6–0.7 (blocks glow from within)
- Opacity: 0.80 (transparent)
- Roughness: 0.05, Metalness: 0.05 (very glossy)
- Transmission: 0.25 (light passes through, glass-like refraction)
- Render both sides (DoubleSide)
- Depth write: OFF (so overlapping transparent blocks don't clip each other)

## EDGE WIREFRAME
- Rendered as LineSegments over the same cube geometry
- Color: same hue as block but 2.5–3.5× brighter (almost white-hot)
- Fully opaque — this is what makes blocks "pop" visually

## PIECE COLOR PALETTE (one color per tetromino type)
- I  →  #4CC9F0  (cyan)
- O  →  #F72585  (hot pink)
- T  →  #7209B7  (purple)
- S  →  #4361EE  (electric blue)
- Z  →  #06D6A0  (teal/mint)
- L  →  #F77F00  (orange)
- J  →  #A855F7  (violet)

## GHOST PIECE (landing preview)
- Same cube geometry
- Face: white, opacity 0.08
- Edges: white, opacity 0.20
- Indicates where the current piece will land

## GLOW ANIMATION
- Emissive intensity pulses over time: 0.4 + sin(time × 2) × 0.15
- Creates a slow "breathing" glow effect on all placed blocks

## LIGHTING SETUP
- Ambient light: white, intensity 0.25–0.55
- Point light 1: #4CC9F0 (cyan), intensity ~4, orbits slowly around the tower
- Point light 2: #F72585 (pink), intensity ~3.5, orbits on the opposite side
- Point light 3: #7209B7 (purple), intensity ~2, positioned near the base
- Lights 1 & 2 animate: x = sin/cos(time × 0.3–0.4) × 5 + offset
- Light intensities also pulse slightly over time

## BACKGROUND & SCENE
- Background color: #06030F (very dark navy-black)
- Exponential fog: same color, density ~0.018
- Tower wireframe outline: white, opacity 0.12 (shows the play-field boundary)
- Floor plane: dark purple #220A40, emissive glow, opacity 0.9
- Large halo plane under tower: #4361EE, opacity 0.06, extends 4× tower width
- 250 background floating particles: cyan/pink/purple, size 0.06, additive blending, slow Y-axis rotation

## PARTICLE EFFECTS (line clear)
- On each cleared layer: emit ~64 particles spread across the X×Z plane
- Each particle: random velocity (outward + upward), fades over ~0.8s
- Colors: random from the piece palette
- Rendered as Points with additive blending, size 0.18

## IMPLEMENTATION NOTES FOR C++/OPENGL
- Translucent faces: enable GL_BLEND, use GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA; draw transparent objects last, back-to-front
- Edge wireframe: draw the same cube with glPolygonMode(GL_FRONT_AND_BACK, GL_LINE) or a separate line-loop pass, slightly scaled up (1.02×) to avoid z-fighting
- Emissive glow: in your fragment shader, add `fragColor += emissiveColor * emissiveIntensity` on top of the Phong/PBR result
- Breathing pulse: pass a `uniform float uTime` to the shader, compute `emissiveIntensity = 0.4 + sin(uTime * 2.0) * 0.15`
- Additive blending for particles: GL_SRC_ALPHA / GL_ONE (additive mode, no depth write)
- Depth write off for transparent blocks: glDepthMask(GL_FALSE) before drawing, glDepthMask(GL_TRUE) after
