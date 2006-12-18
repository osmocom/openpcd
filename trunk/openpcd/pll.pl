#!/usr/bin/perl


# MCLK = (18.432 / DIV) * MUL = 48.000

my $MCLK = 18432;

my $div;
my $mul;

for ($div = 1; $div < 256; $div++) {
	my $tmp = $MCLK / $div;
	for ($mul = 1; $mul < 2048; $mul++) {
		my $res = $tmp * $mul;
		if ($res == 96000) {
			printf("res=%u, div=%u, tmp=%u, mul=%u\n", 
				$res, $div, $tmp, $mul);
		}
	}
}


