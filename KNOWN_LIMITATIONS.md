# Known limitations

## Model

Factual hallucinations, omissions, false-premise acceptance, correction/ambiguity failures, weak procedures/recipes, uncertainty handling and constrained-output failures remain. Calculator/reference routing does not make neural responses generally reliable.

## Runtime/performance

Long prompts can be slow. Sessions are bounded to 512 positions; continuation is bounded. Upstream context claims do not apply to Vita. Some statistics retain earlier/last-segment values, including historical Stats-after-Clear behavior. Endurance, battery/energy use and temperature are not established.

## UI/font

Raster Unicode coverage remains limited despite added punctuation. Theme is process-local and defaults Dark. Texture allocation failure falls back to flat rendering.

## Evaluation

Quality scores are provisional agent review, not independent blind human validation. Phase12 clean conversation acceptance remains 4/5 checks, 3/4 cases. Eval80-075 actual-input trace remains unrun. Exact physical output retention does not prove device hidden-state equality or every conversation correct. Branding did not fix historical semantic failures.
