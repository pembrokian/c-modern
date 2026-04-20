enum ActiveRegion {
    Body,
    Status,
}

func active_region_toggle(region: ActiveRegion) ActiveRegion {
    switch region {
    case ActiveRegion.Status:
        return ActiveRegion.Body
    default:
        return ActiveRegion.Status
    }
}