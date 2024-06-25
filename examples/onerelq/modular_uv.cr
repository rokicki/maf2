_RWS_Cos_and := rec
(
  isFSA := true,
  alphabet := rec
  (
    type := "identifiers",
    size := 4,
    format := "dense",
    names  := [u,U,v,V]
  ),
  states := rec
  (
    type := "simple",
    size := 8
  ),
  flags := ["DFA","minimized","accessible","trim","BFS"],
  initial := [1],
  accepting := [1..7],
  table := rec
  (
    format := "dense deterministic",
    transitions :=
    [
      [2,0,0,0],
      [3,0,4,5],
      [3,0,4,6],
      [4,0,4,0],
      [7,8,0,6],
      [0,8,0,6],
      [0,0,0,0],
      [0,8,0,6]
    ]
  )
);
