# test_randomFunctions.praat
# djmw 20121211, 20191227


appendInfoLine: "test_randomFunctions.praat"

Erase all
@test_randomBiniomial
@test_randomGamma: 4, 0.03
sleep: 2	; some time to look at the drawing

appendInfoLine:  "test_randomFunctions.praat OK"

procedure test_randomBiniomial
	appendInfoLine: tab$,  "randomBinomial"
	.p =1/6
	.n = 100
	.np = .n * .p
	.numberOfRuns = 100000
	appendInfoLine:   tab$, tab$, .numberOfRuns, " runs with randomBinomial (", .p, ",", .n, ")"
	appendInfoLine:   tab$, tab$, "We expect values between 0 and ", .n, " with a mean near n * p = ", .np
	appendInfoLine:   tab$, tab$, "Bin 1 counts number of times randomBinomial return 0, bin 2 number of times randomBinomial returns 1,  etc."
	appendInfoLine:   tab$, tab$, "The probability q of the values in bin i equals binomialP (p, i - 1 , n) - binomialP (p, i - 2 , n)"
	appendInfoLine:   tab$, tab$,  "In bin i we expect numberOfRuns * q (=", .numberOfRuns, " * q  values"
	.mat = Create simple Matrix: "m", .numberOfRuns, 1, "randomBinomial (.p, .n)"
	Select outer viewport: 0, 6, 0, 3
	.numberOfBins = 100
	Draw distribution: 0, 0, 0, 0, 0, 0, .numberOfBins, 0, 0, "yes"
	One mark bottom: .np, "yes", "yes", "yes", ""
	Text top: "no", "randomBinomial (1/6, 100)"
	.table = Create TableOfReal: "t", 3, .n+1
	for .i to .numberOfRuns
		.nb = randomBinomial (.p, .n)
		.val = Get value: 1, .nb+1
		.val += 1
		Set value: 1, .nb+1, .val
	endfor

	for .i from 2 to .n+1
		.val = Get value: 1, .i
		.q = binomialP (.p, .i - 1 , .n) - binomialP (.p, .i - 2 , .n)
		.expect = round (.numberOfRuns * .q)
		.diff = .val - .expect
		Set value: 2, .i,  .expect
		Set value: 3, .i, .diff
		appendInfoLine: tab$, tab$, "bin = ", .i, ", diff = ", .diff, "  (expected = ", .expect,  " q = ", .q
	endfor

	removeObject: .mat, .table
endproc

procedure test_randomGamma: .alpha, .beta
	appendInfoLine: tab$,  "randomGamma"
	.nrow = 1000
	.ncol = 100
	.gamma = Create simple Matrix: "gamma", .nrow, .ncol, "randomGamma (.alpha, .beta)"
	Select outer viewport: 0, 6, 3, 6
	.numberOfBins = 100
	.maximumValue = 750
	Draw distribution: 0, 0, 0, 0, 0, .maximumValue, .numberOfBins, 0, 0, "yes"
	.xOfMaximum = (.alpha - 1) / .beta
	One mark bottom: .xOfMaximum, "yes", "yes", "yes", ""
	.numberOfCells = .nrow * .ncol
	Draw function: 0, .maximumValue, 1000, "(.maximumValue/.numberOfBins)*
	... .numberOfCells*exp(-lnGamma(.alpha))*.beta^.alpha *x^(.alpha-1)*exp(-.beta*x)"
	.topText$ = "randomGamma (" + string$( .alpha) + ", " + string$ (.beta) +  ")"
	Text top: "no", .topText$
endproc

