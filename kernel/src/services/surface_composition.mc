import display_surface

const SURFACE_REGION_CELL_COUNT: usize = 2

struct SurfaceComposition {
    body: [SURFACE_REGION_CELL_COUNT]u8
    status: [SURFACE_REGION_CELL_COUNT]u8
}

func surface_composition(body: [SURFACE_REGION_CELL_COUNT]u8, status: [SURFACE_REGION_CELL_COUNT]u8) SurfaceComposition {
    return SurfaceComposition{ body: body, status: status }
}

func compose_cells(composition: SurfaceComposition) [display_surface.DISPLAY_CELL_COUNT]u8 {
    return [display_surface.DISPLAY_CELL_COUNT]u8{
        composition.body[0],
        composition.body[1],
        composition.status[0],
        composition.status[1]
    }
}

func compose_present(display: display_surface.DisplaySurfaceState, composition: SurfaceComposition) display_surface.DisplayResult {
    return display_surface.display_present(display, compose_cells(composition))
}