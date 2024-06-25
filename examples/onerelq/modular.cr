_RWS.wa := rec
(
  isFSA := true,
  alphabet := rec
  (
    type := "identifiers",
    size := 3,
    format := "dense",
    names  := [x,y,Y]
  ),
  states := rec
  (
    type := "simple",
    size := 4
  ),
  flags := ["DFA","minimized","accessible","trim","BFS"],
  initial := [1],
  accepting := [2..3],
  table := rec
  (
    format := "dense deterministic",
    transitions :=
    [
      [2,0,0],
      [0,3,3],
      [4,0,0],
      [0,3,3]
    ]
  )
);
