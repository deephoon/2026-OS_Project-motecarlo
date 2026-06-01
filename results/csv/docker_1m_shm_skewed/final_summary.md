# Final Experiment Summary

## Conditions

- trials: 1000000
- steps: 50
- repeats: 5
- workload: skewed
- skew_factor: 8
- pre_work: 50000
- post_work: 10000
- sequential baseline avg: 0.128468s

## OS26 Guide Coverage

| Requirement | Evidence in this run |
| --- | --- |
| sequential baseline | `seq` case |
| single/multi thread | `thread_1_reduce`, `thread_2_reduce`, `thread_4_reduce`, `thread_8_reduce` |
| child process | `process_1_pipe`, `process_2_pipe`, `process_4_pipe`, and shm variants |
| hybrid process + thread | `hybrid_2x2`, `hybrid_2x4`, `hybrid_4x2`, `hybrid_2x4_shm` |
| synchronization comparison | `thread_4_nosync`, `thread_4_mutex`, `thread_4_reduce` |
| pipeline and merge strategy | `pipeline_final_b1000`, `pipeline_interactive_b1000` |
| batch granularity | `pipeline_interactive_b100`, `pipeline_interactive_b1000`, `pipeline_interactive_b10000` |

## Fastest Cases By Average Time

| Case | Avg time | Speedup | Efficiency | Valid | Checksum matches seq |
| --- | ---: | ---: | ---: | ---: | ---: |
| hybrid_4x2 | 0.035298 | 3.640 | 0.455 | 1 | 1 |
| hybrid_2x4_shm | 0.035857 | 3.583 | 0.448 | 1 | 1 |
| hybrid_2x4 | 0.035894 | 3.579 | 0.447 | 1 | 1 |
| pipeline_final_b1000 | 0.036921 | 3.480 | 0.870 | 1 | 1 |
| thread_8_reduce | 0.037207 | 3.453 | 0.432 | 1 | 1 |

## Validation Risks

| Case | Valid all | Checksum count | Matches seq checksum |
| --- | ---: | ---: | ---: |
| thread_4_nosync | 0 | 5 | 0 |

## Report Guidance

- Treat `thread_4_nosync` as a synchronization failure demonstration if checksum differs.
- Use `speedup_vs_seq_avg` and `efficiency_vs_seq_avg` from `final_analyzed.csv`, not the raw per-run zero placeholders.
- Explain `time_sync` as observed synchronization/IPC wait cost. For queue modes it can include accumulated worker wait time.
- Use large `TRIALS`, large `STEPS`, and at least 5 repeats for final report claims.
