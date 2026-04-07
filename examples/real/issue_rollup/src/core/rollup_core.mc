import rollup_model
import rollup_parse
import rollup_render

func summarize_text(text: str) rollup_model.Summary {
    return rollup_parse.summarize_text(text)
}

func write_text_rollup(text: str) i32 {
    summary: rollup_model.Summary = summarize_text(text)
    return rollup_render.write_rollup(summary)
}