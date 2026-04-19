enum ProgramLaunchKind {
    None,
    HostedExe,
}

const PROGRAM_DISPLAY_NONE: [4]u8 = { 0, 0, 0, 0 }
const ISSUE_ROLLUP_DISPLAY: [4]u8 = { 73, 82, 85, 80 }
const REVIEW_BOARD_DISPLAY: [4]u8 = { 82, 66, 82, 68 }

struct ProgramLaunchDescriptor {
    package: str
    target: str
    kind: ProgramLaunchKind
}

struct ProgramDescriptor {
    id: u8
    label: str
    launch: ProgramLaunchDescriptor
}

const PROGRAM_ID_NONE: u8 = 0
const PROGRAM_ID_ISSUE_ROLLUP: u8 = 1
const PROGRAM_ID_REVIEW_BOARD: u8 = 2

const PROGRAM_KIND_CODE_NONE: u8 = 0
const PROGRAM_KIND_CODE_HOSTED_EXE: u8 = 69

const INVALID_PROGRAM_LAUNCH: ProgramLaunchDescriptor = {
    package: "",
    target: "",
    kind: ProgramLaunchKind.None
}

const INVALID_PROGRAM_DESCRIPTOR: ProgramDescriptor = {
    id: PROGRAM_ID_NONE,
    label: "",
    launch: INVALID_PROGRAM_LAUNCH
}

const ISSUE_ROLLUP_PROGRAM: ProgramDescriptor = {
    id: PROGRAM_ID_ISSUE_ROLLUP,
    label: "issue_rollup",
    launch: {
        package: "issue-rollup",
        target: "issue-rollup",
        kind: ProgramLaunchKind.HostedExe
    }
}

const REVIEW_BOARD_PROGRAM: ProgramDescriptor = {
    id: PROGRAM_ID_REVIEW_BOARD,
    label: "review_board",
    launch: {
        package: "review-board",
        target: "audit",
        kind: ProgramLaunchKind.HostedExe
    }
}

const PROGRAM_DESCRIPTORS: [2]ProgramDescriptor = {
    ISSUE_ROLLUP_PROGRAM,
    REVIEW_BOARD_PROGRAM
}

func program_count() usize {
    return 2
}

func program_descriptor_at(index: usize) ProgramDescriptor {
    if index >= program_count() {
        return INVALID_PROGRAM_DESCRIPTOR
    }
    return PROGRAM_DESCRIPTORS[index]
}

func program_descriptor_for_id(id: u8) ProgramDescriptor {
    for i in 0..program_count() {
        desc := program_descriptor_at(i)
        if desc.id == id {
            return desc
        }
    }
    return INVALID_PROGRAM_DESCRIPTOR
}

func program_descriptor_is_valid(desc: ProgramDescriptor) bool {
    return desc.id != PROGRAM_ID_NONE
}

func program_launch_kind_code(desc: ProgramDescriptor) u8 {
    switch desc.launch.kind {
    case ProgramLaunchKind.HostedExe:
        return PROGRAM_KIND_CODE_HOSTED_EXE
    default:
        return PROGRAM_KIND_CODE_NONE
    }
}

func program_display_cells(id: u8) [4]u8 {
    switch id {
    case PROGRAM_ID_ISSUE_ROLLUP:
        return ISSUE_ROLLUP_DISPLAY
    case PROGRAM_ID_REVIEW_BOARD:
        return REVIEW_BOARD_DISPLAY
    default:
        return PROGRAM_DISPLAY_NONE
    }
}
