module ExpandedAsic (
	clk,
	reset,
	native_start,
	native_ready,
	fused,
	phase,
	native_operation,
	length,
	divisor,
	nodes,
	row,
	write,
	bank,
	address,
	wdata,
	rdata,
	in_valid,
	in_ready,
	left,
	right,
	out_valid,
	out_ready,
	out_data,
	native_busy,
	native_done,
	cmd_valid,
	cmd_ready,
	mask,
	operation,
	a,
	b,
	c,
	d,
	rsp_valid,
	rsp_ready,
	result
);
	parameter integer FAMILY = 0;
	input wire clk;
	input wire reset;
	input wire native_start;
	output wire native_ready;
	input wire fused;
	input wire [2:0] phase;
	input wire [1:0] native_operation;
	input wire [31:0] length;
	input wire [31:0] divisor;
	input wire [15:0] nodes;
	input wire row;
	input wire write;
	input wire [1:0] bank;
	input wire [10:0] address;
	input wire [31:0] wdata;
	output wire [31:0] rdata;
	input wire in_valid;
	output wire in_ready;
	input wire [31:0] left;
	input wire [31:0] right;
	output wire out_valid;
	input wire out_ready;
	output wire [31:0] out_data;
	output reg native_busy;
	output reg native_done;
	input wire cmd_valid;
	output wire cmd_ready;
	input wire [15:0] mask;
	input wire [79:0] operation;
	input wire [511:0] a;
	input wire [511:0] b;
	input wire [511:0] c;
	input wire [511:0] d;
	output wire rsp_valid;
	input wire rsp_ready;
	output wire [511:0] result;
	wire accept_native;
	wire bank_busy;
	wire bank_valid;
	wire bank_ready;
	wire native_write;
	wire bank_rsp_valid;
	wire bank_rsp_ready;
	wire [15:0] bank_mask;
	wire [79:0] bank_operation;
	wire [511:0] bank_a;
	wire [511:0] bank_b;
	wire [511:0] bank_c;
	wire [511:0] bank_d;
	wire [511:0] bank_result;
	wire div_start;
	wire div_signed;
	wire div_busy;
	wire div_done;
	wire [31:0] div_a;
	wire [31:0] div_b;
	wire [31:0] div_result;
	assign native_ready = (!native_busy && !bank_busy) && !cmd_valid;
	assign accept_native = native_start && native_ready;
	assign native_write = (write && !bank_busy) && !cmd_valid;
	assign cmd_ready = !native_busy && bank_ready;
	assign rsp_valid = !native_busy && bank_rsp_valid;
	assign result = bank_result;
	PrimitiveBank primitives(
		.clk(clk),
		.reset(reset),
		.cmd_valid(bank_valid),
		.cmd_ready(bank_ready),
		.mask(bank_mask),
		.operation(bank_operation),
		.a(bank_a),
		.b(bank_b),
		.c(bank_c),
		.d(bank_d),
		.rsp_valid(bank_rsp_valid),
		.rsp_ready(bank_rsp_ready),
		.result(bank_result),
		.busy(bank_busy),
		.native_start(div_start),
		.native_signed(div_signed),
		.native_a(div_a),
		.native_b(div_b),
		.native_busy(div_busy),
		.native_done(div_done),
		.native_result(div_result)
	);
	generate
		if ((FAMILY == 0) || (FAMILY == 1)) begin : streaming
			reg [31:0] size;
			reg [31:0] received;
			reg [31:0] sent;
			assign rdata = 0;
			assign out_valid = native_busy && bank_rsp_valid;
			assign out_data = bank_result[31:0];
			assign in_ready = (native_busy && (received < size)) && bank_ready;
			assign bank_valid = (native_busy ? in_valid && (received < size) : cmd_valid);
			assign bank_mask = (native_busy ? 16'h0001 : mask);
			assign bank_operation = (native_busy ? (FAMILY == 0 ? {16 {5'd0}} : {16 {5'd20}}) : operation);
			assign bank_a = (native_busy ? {480'd0, left} : a);
			assign bank_b = (native_busy ? {480'd0, right} : b);
			assign bank_c = (native_busy ? 512'd0 : c);
			assign bank_d = (native_busy ? 512'd0 : d);
			assign bank_rsp_ready = (native_busy ? out_ready : rsp_ready);
			assign div_start = 0;
			assign div_signed = 0;
			assign div_a = 0;
			assign div_b = 0;
			always @(posedge clk) begin
				native_done <= 0;
				if (reset) begin
					native_busy <= 0;
					size <= 0;
					received <= 0;
					sent <= 0;
				end
				else begin
					if (accept_native) begin
						native_busy <= 1;
						size <= length;
						received <= 0;
						sent <= 0;
					end
					if (in_valid && in_ready)
						received <= received + 1'b1;
					if (out_valid && out_ready) begin
						sent <= sent + 1'b1;
						if ((sent + 1'b1) == size) begin
							native_busy <= 0;
							native_done <= 1;
						end
					end
				end
			end
		end
		else if (FAMILY == 2) begin : gcn
			wire arith_valid;
			wire arith_rsp_ready;
			wire [79:0] arith_operation;
			wire [511:0] arith_a;
			wire [511:0] arith_b;
			assign rdata = 0;
			assign in_ready = 0;
			assign bank_valid = (native_busy ? arith_valid : cmd_valid);
			assign bank_mask = (native_busy ? 16'hffff : mask);
			assign bank_operation = (native_busy ? arith_operation : operation);
			assign bank_a = (native_busy ? arith_a : a);
			assign bank_b = (native_busy ? arith_b : b);
			assign bank_c = (native_busy ? 512'd0 : c);
			assign bank_d = (native_busy ? 512'd0 : d);
			assign bank_rsp_ready = (native_busy ? arith_rsp_ready : rsp_ready);
			assign div_start = 0;
			assign div_signed = 0;
			assign div_a = 0;
			assign div_b = 0;
			wire [1:1] sv2v_tmp_kernel_busy;
			always @(*) native_busy = sv2v_tmp_kernel_busy;
			wire [1:1] sv2v_tmp_kernel_done;
			always @(*) native_done = sv2v_tmp_kernel_done;
			GcnNative kernel(
				.clk(clk),
				.reset(reset),
				.load(native_write),
				.bank(bank),
				.address(address),
				.data(wdata),
				.start(accept_native),
				.fused(fused),
				.nodes(nodes),
				.busy(sv2v_tmp_kernel_busy),
				.done(sv2v_tmp_kernel_done),
				.out_valid(out_valid),
				.out_ready(out_ready),
				.out_data(out_data),
				.arith_valid(arith_valid),
				.arith_ready(bank_ready),
				.arith_operation(arith_operation),
				.arith_a(arith_a),
				.arith_b(arith_b),
				.arith_result(bank_result),
				.arith_rsp_valid(bank_rsp_valid),
				.arith_rsp_ready(arith_rsp_ready)
			);
		end
		else begin : gpt
			assign in_ready = 0;
			assign out_valid = 0;
			assign out_data = 0;
			assign bank_valid = cmd_valid && !native_busy;
			assign bank_mask = mask;
			assign bank_operation = operation;
			assign bank_a = a;
			assign bank_b = b;
			assign bank_c = c;
			assign bank_d = d;
			assign bank_rsp_ready = rsp_ready && !native_busy;
			if (FAMILY == 3) begin : norm
				wire [1:1] sv2v_tmp_kernel_busy;
				always @(*) native_busy = sv2v_tmp_kernel_busy;
				wire [1:1] sv2v_tmp_kernel_done;
				always @(*) native_done = sv2v_tmp_kernel_done;
				GptNormNative kernel(
					.clk(clk),
					.reset(reset),
					.start(accept_native),
					.fused(fused),
					.phase(phase),
					.write(native_write),
					.bank(bank),
					.address(address[9:0]),
					.wdata(wdata),
					.rdata(rdata),
					.busy(sv2v_tmp_kernel_busy),
					.done(sv2v_tmp_kernel_done),
					.div_start(div_start),
					.div_signed(div_signed),
					.div_a(div_a),
					.div_b(div_b),
					.div_busy(div_busy),
					.div_done(div_done),
					.div_result(div_result)
				);
			end
			else if (FAMILY == 4) begin : softmax
				wire [1:1] sv2v_tmp_kernel_busy;
				always @(*) native_busy = sv2v_tmp_kernel_busy;
				wire [1:1] sv2v_tmp_kernel_done;
				always @(*) native_done = sv2v_tmp_kernel_done;
				GptSoftmaxNative kernel(
					.clk(clk),
					.reset(reset),
					.start(accept_native),
					.fused(fused),
					.phase(phase),
					.row(row),
					.write(native_write),
					.address(address[9:0]),
					.wdata(wdata),
					.rdata(rdata),
					.busy(sv2v_tmp_kernel_busy),
					.done(sv2v_tmp_kernel_done),
					.div_start(div_start),
					.div_signed(div_signed),
					.div_a(div_a),
					.div_b(div_b),
					.div_busy(div_busy),
					.div_done(div_done),
					.div_result(div_result)
				);
			end
			else begin : vector_unit
				wire [1:1] sv2v_tmp_kernel_busy;
				always @(*) native_busy = sv2v_tmp_kernel_busy;
				wire [1:1] sv2v_tmp_kernel_done;
				always @(*) native_done = sv2v_tmp_kernel_done;
				GptVectorNative kernel(
					.clk(clk),
					.reset(reset),
					.start(accept_native),
					.operation(native_operation),
					.count(length[10:0]),
					.divisor(divisor),
					.write(native_write),
					.bank(bank[0]),
					.address(address[9:0]),
					.wdata(wdata),
					.rdata(rdata),
					.busy(sv2v_tmp_kernel_busy),
					.done(sv2v_tmp_kernel_done),
					.div_start(div_start),
					.div_signed(div_signed),
					.div_a(div_a),
					.div_b(div_b),
					.div_busy(div_busy),
					.div_done(div_done),
					.div_result(div_result)
				);
			end
		end
	endgenerate
endmodule
module Restoring (
	clk,
	reset,
	start,
	signed_mode,
	mode,
	a,
	b,
	busy,
	done,
	result
);
	reg _sv2v_0;
	input wire clk;
	input wire reset;
	input wire start;
	input wire signed_mode;
	input wire [1:0] mode;
	input wire [31:0] a;
	input wire [31:0] b;
	output reg busy;
	output reg done;
	output reg [31:0] result;
	reg [31:0] quotient;
	reg [31:0] divisor;
	reg [32:0] remainder;
	reg [32:0] trial;
	reg [32:0] subtract;
	reg [31:0] next_quotient;
	reg [32:0] next_remainder;
	reg [5:0] count;
	reg square_root;
	reg take_remainder;
	reg negative;
	always @(*) begin
		if (_sv2v_0)
			;
		trial = {remainder[31:0], quotient[31]};
		subtract = {1'b0, divisor};
		if (square_root) begin
			trial = (remainder << 2) | {31'd0, divisor[31:30]};
			subtract = ({1'b0, quotient} << 2) | 33'd1;
		end
		next_remainder = (trial >= subtract ? trial - subtract : trial);
		next_quotient = {quotient[30:0], trial >= subtract};
	end
	always @(posedge clk) begin
		done <= 0;
		if (reset) begin
			busy <= 0;
			result <= 0;
			quotient <= 0;
			divisor <= 0;
			remainder <= 0;
			count <= 0;
			square_root <= 0;
			take_remainder <= 0;
			negative <= 0;
		end
		else if (start && !busy) begin
			busy <= 1;
			square_root <= mode == 2;
			take_remainder <= mode == 1;
			negative <= signed_mode && (a[31] != b[31]);
			quotient <= (mode == 2 ? 32'd0 : (signed_mode && a[31] ? -a : a));
			divisor <= (mode == 2 ? a : (signed_mode && b[31] ? -b : b));
			remainder <= 0;
			count <= (mode == 2 ? 6'd16 : 6'd32);
		end
		else if (busy) begin
			quotient <= next_quotient;
			remainder <= next_remainder;
			if (square_root)
				divisor <= divisor << 2;
			count <= count - 1'b1;
			if (count == 1) begin
				busy <= 0;
				done <= 1;
				result <= (take_remainder ? next_remainder[31:0] : (negative ? -next_quotient : next_quotient));
			end
		end
	end
	initial _sv2v_0 = 0;
endmodule
module ArithmeticLane (
	clk,
	reset,
	in_valid,
	in_ready,
	operation,
	a,
	b,
	c,
	d,
	out_valid,
	out_ready,
	out_data,
	native_start,
	native_signed,
	native_a,
	native_b,
	native_busy,
	native_done,
	native_result
);
	reg _sv2v_0;
	input wire clk;
	input wire reset;
	input wire in_valid;
	output wire in_ready;
	input wire [4:0] operation;
	input wire [31:0] a;
	input wire [31:0] b;
	input wire [31:0] c;
	input wire [31:0] d;
	output wire out_valid;
	input wire out_ready;
	output reg [31:0] out_data;
	input wire native_start;
	input wire native_signed;
	input wire [31:0] native_a;
	input wire [31:0] native_b;
	output wire native_busy;
	output wire native_done;
	output wire [31:0] native_result;
	localparam [4:0] ADD = 0;
	localparam [4:0] SUB = 1;
	localparam [4:0] MUL = 2;
	localparam [4:0] DIV = 3;
	localparam [4:0] REM = 4;
	localparam [4:0] SQRT = 5;
	localparam [4:0] AND_OP = 6;
	localparam [4:0] OR_OP = 7;
	localparam [4:0] XOR_OP = 8;
	localparam [4:0] BIT_NOT = 9;
	localparam [4:0] LOG_NOT = 10;
	localparam [4:0] SHL = 11;
	localparam [4:0] SHR = 12;
	localparam [4:0] SAR = 13;
	localparam [4:0] EQ = 14;
	localparam [4:0] NE = 15;
	localparam [4:0] LT = 16;
	localparam [4:0] GE = 17;
	localparam [4:0] GT = 18;
	localparam [4:0] SELECT = 19;
	localparam [4:0] RELU = 20;
	localparam [4:0] PASS = 21;
	localparam [4:0] INC = 22;
	localparam [4:0] GEP2D = 23;
	reg [2:0] state;
	reg [4:0] op;
	reg [31:0] x;
	reg [31:0] y;
	reg [31:0] z;
	reg [31:0] stride;
	reg [31:0] partial;
	reg [31:0] fast_value;
	wire [31:0] multiply;
	wire accept;
	wire iterative;
	wire slow_start;
	wire [1:0] slow_mode;
	assign in_ready = ((state == 3'd0) && !native_busy) && !native_start;
	assign accept = in_valid && in_ready;
	assign iterative = ((operation == DIV) || (operation == REM)) || (operation == SQRT);
	assign slow_start = native_start || (accept && iterative);
	assign slow_mode = (native_start ? 2'd0 : (operation == SQRT ? 2'd2 : (operation == REM ? 2'd1 : 2'd0)));
	Restoring divider(
		.clk(clk),
		.reset(reset),
		.start(slow_start),
		.signed_mode(native_start && native_signed),
		.mode(slow_mode),
		.a((native_start ? native_a : a)),
		.b((native_start ? native_b : b)),
		.busy(native_busy),
		.done(native_done),
		.result(native_result)
	);
	assign out_valid = state == 3'd5;
	assign multiply = y * (op == GEP2D ? stride : x);
	always @(*) begin
		if (_sv2v_0)
			;
		fast_value = 0;
		case (op)
			ADD: fast_value = x + y;
			SUB: fast_value = x - y;
			MUL: fast_value = multiply;
			AND_OP: fast_value = x & y;
			OR_OP: fast_value = x | y;
			XOR_OP: fast_value = x ^ y;
			BIT_NOT: fast_value = ~x;
			LOG_NOT: fast_value = {31'd0, x == 0};
			SHL: fast_value = x << y;
			SHR: fast_value = x >> y;
			SAR: fast_value = $signed(x) >>> y;
			EQ: fast_value = {31'd0, x == y};
			NE: fast_value = {31'd0, x != y};
			LT: fast_value = {31'd0, $signed(x) < $signed(y)};
			GE: fast_value = {31'd0, $signed(x) >= $signed(y)};
			GT: fast_value = {31'd0, $signed(x) > $signed(y)};
			SELECT: fast_value = (z == 32'd1 ? x : y);
			RELU: fast_value = (x[31] ? 32'd0 : x);
			PASS: fast_value = x;
			INC: fast_value = x + 1'b1;
			default:
				;
		endcase
	end
	always @(posedge clk)
		if (reset) begin
			state <= 3'd0;
			op <= 0;
			x <= 0;
			y <= 0;
			z <= 0;
			stride <= 0;
			partial <= 0;
			out_data <= 0;
		end
		else
			case (state)
				3'd0:
					if (accept) begin
						op <= operation;
						x <= a;
						y <= b;
						z <= c;
						stride <= d;
						state <= (iterative ? 3'd2 : 3'd1);
					end
				3'd1:
					if (op == GEP2D) begin
						partial <= multiply;
						state <= 3'd3;
					end
					else begin
						out_data <= fast_value;
						state <= 3'd5;
					end
				3'd3: begin
					partial <= partial + x;
					state <= 3'd4;
				end
				3'd4: begin
					out_data <= partial + z;
					state <= 3'd5;
				end
				3'd2:
					if (native_done) begin
						out_data <= native_result;
						state <= 3'd5;
					end
				3'd5:
					if (out_ready)
						state <= 3'd0;
				default: state <= 3'd0;
			endcase
	initial _sv2v_0 = 0;
endmodule
module PrimitiveBank (
	clk,
	reset,
	cmd_valid,
	cmd_ready,
	mask,
	operation,
	a,
	b,
	c,
	d,
	rsp_valid,
	rsp_ready,
	result,
	busy,
	native_start,
	native_signed,
	native_a,
	native_b,
	native_busy,
	native_done,
	native_result
);
	input wire clk;
	input wire reset;
	input wire cmd_valid;
	output wire cmd_ready;
	input wire [15:0] mask;
	input wire [79:0] operation;
	input wire [511:0] a;
	input wire [511:0] b;
	input wire [511:0] c;
	input wire [511:0] d;
	output wire rsp_valid;
	input wire rsp_ready;
	output wire [511:0] result;
	output reg busy;
	input wire native_start;
	input wire native_signed;
	input wire [31:0] native_a;
	input wire [31:0] native_b;
	output wire native_busy;
	output wire native_done;
	output wire [31:0] native_result;
	reg [15:0] active;
	wire [15:0] ready;
	wire [15:0] valid;
	wire accepted;
	assign cmd_ready = !busy && &ready;
	assign accepted = cmd_valid && cmd_ready;
	assign rsp_valid = busy && &(valid | ~active);
	genvar _gv_i_1;
	generate
		for (_gv_i_1 = 0; _gv_i_1 < 16; _gv_i_1 = _gv_i_1 + 1) begin : lane
			localparam i = _gv_i_1;
			wire unused_busy;
			wire unused_done;
			wire [31:0] unused_result;
			ArithmeticLane unit(
				.clk(clk),
				.reset(reset),
				.in_valid(accepted && mask[i]),
				.in_ready(ready[i]),
				.operation(operation[i * 5+:5]),
				.a(a[i * 32+:32]),
				.b(b[i * 32+:32]),
				.c(c[i * 32+:32]),
				.d(d[i * 32+:32]),
				.out_valid(valid[i]),
				.out_ready(rsp_valid && rsp_ready),
				.out_data(result[i * 32+:32]),
				.native_start((i == 0) && native_start),
				.native_signed(native_signed),
				.native_a(native_a),
				.native_b(native_b),
				.native_busy(unused_busy),
				.native_done(unused_done),
				.native_result(unused_result)
			);
			if (i == 0) begin : shared_divider
				assign native_busy = unused_busy;
				assign native_done = unused_done;
				assign native_result = unused_result;
			end
		end
	endgenerate
	always @(posedge clk)
		if (reset) begin
			busy <= 0;
			active <= 0;
		end
		else begin
			if (accepted) begin
				busy <= 1;
				active <= mask;
			end
			if (rsp_valid && rsp_ready)
				busy <= 0;
		end
endmodule
module GcnNative (
	clk,
	reset,
	load,
	bank,
	address,
	data,
	start,
	fused,
	nodes,
	busy,
	done,
	out_valid,
	out_ready,
	out_data,
	arith_valid,
	arith_ready,
	arith_operation,
	arith_a,
	arith_b,
	arith_result,
	arith_rsp_valid,
	arith_rsp_ready
);
	input wire clk;
	input wire reset;
	input wire load;
	input wire [1:0] bank;
	input wire [10:0] address;
	input wire [31:0] data;
	input wire start;
	input wire fused;
	input wire [15:0] nodes;
	output wire busy;
	output reg done;
	output wire out_valid;
	input wire out_ready;
	output wire signed [31:0] out_data;
	output wire arith_valid;
	input wire arith_ready;
	output wire [79:0] arith_operation;
	output wire [511:0] arith_a;
	output wire [511:0] arith_b;
	input wire [511:0] arith_result;
	input wire arith_rsp_valid;
	output wire arith_rsp_ready;
	localparam integer LANES = 16;
	localparam integer READS = 7;
	reg [31:0] rowptr [0:16];
	reg [31:0] columns [0:255];
	wire [31:0] feature_values [0:LANES - 1];
	wire [31:0] weight_values [0:1];
	reg [3:0] state;
	reg [31:0] row;
	reg [31:0] channel;
	reg [31:0] edge_index;
	reg [31:0] edge_start;
	reg [31:0] edge_end;
	reg [31:0] column;
	reg [15:0] rows;
	reg activate;
	reg [4:0] read_base;
	reg [3:0] emit_lane;
	reg signed [31:0] weight;
	reg signed [31:0] value [0:LANES - 1];
	reg signed [31:0] product [0:LANES - 1];
	reg signed [31:0] accumulator [0:LANES - 1];
	wire signed [31:0] sum [0:LANES - 1];
	reg signed [31:0] result [0:LANES - 1];
	assign arith_valid = (state == 4'd5) || (state == 4'd7);
	assign arith_operation = (state == 4'd5 ? {16 {5'd2}} : {16 {5'd0}});
	assign arith_rsp_ready = (state == 4'd6) || (state == 4'd8);
	assign busy = state != 4'd0;
	assign out_valid = state == 4'd9;
	assign out_data = result[emit_lane];
	genvar _gv_i_2;
	function automatic signed [3:0] sv2v_cast_4_signed;
		input reg signed [3:0] inp;
		sv2v_cast_4_signed = inp;
	endfunction
	function automatic [6:0] sv2v_cast_7;
		input reg [6:0] inp;
		sv2v_cast_7 = inp;
	endfunction
	generate
		for (_gv_i_2 = 0; _gv_i_2 < LANES; _gv_i_2 = _gv_i_2 + 1) begin : lanes
			localparam i = _gv_i_2;
			GcnBank memory(
				.clk(clk),
				.we(((load && !busy) && (bank == 3)) && (address[3:0] == sv2v_cast_4_signed(i))),
				.waddr(address[10:4]),
				.wdata(data),
				.raddr(sv2v_cast_7(((column * 128) + channel) >> 4)),
				.rdata(feature_values[i])
			);
			assign sum[i] = arith_result[i * 32+:32];
			assign arith_a[i * 32+:32] = (state == 4'd5 ? value[i] : accumulator[i]);
			assign arith_b[i * 32+:32] = (state == 4'd5 ? weight : product[i]);
		end
	endgenerate
	genvar _gv_i_3;
	function automatic signed [0:0] sv2v_cast_1_signed;
		input reg signed [0:0] inp;
		sv2v_cast_1_signed = inp;
	endfunction
	generate
		for (_gv_i_3 = 0; _gv_i_3 < 2; _gv_i_3 = _gv_i_3 + 1) begin : weight_banks
			localparam i = _gv_i_3;
			GcnBank memory(
				.clk(clk),
				.we(((load && !busy) && (bank == 2)) && (address[7] == sv2v_cast_1_signed(i))),
				.waddr(address[6:0]),
				.wdata(data),
				.raddr(sv2v_cast_7(edge_index)),
				.rdata(weight_values[i])
			);
		end
	endgenerate
	function automatic [31:0] sv2v_cast_32;
		input reg [31:0] inp;
		sv2v_cast_32 = inp;
	endfunction
	function automatic signed [4:0] sv2v_cast_5_signed;
		input reg signed [4:0] inp;
		sv2v_cast_5_signed = inp;
	endfunction
	always @(posedge clk) begin
		if (load && !busy)
			case (bank)
				2'd0: rowptr[address[4:0]] <= data;
				2'd1: columns[address[7:0]] <= data;
				default:
					;
			endcase
		if (reset) begin
			state <= 4'd0;
			done <= 0;
			row <= 0;
			channel <= 0;
			edge_index <= 0;
			edge_start <= 0;
			edge_end <= 0;
			column <= 0;
			rows <= 0;
			activate <= 0;
			weight <= 0;
			read_base <= 0;
			emit_lane <= 0;
			begin : sv2v_autoblock_1
				integer i;
				for (i = 0; i < LANES; i = i + 1)
					begin
						value[i] <= 0;
						accumulator[i] <= 0;
						result[i] <= 0;
					end
			end
		end
		else begin
			done <= 0;
			case (state)
				4'd0:
					if (start) begin
						rows <= nodes;
						activate <= fused;
						row <= 0;
						channel <= 0;
						state <= 4'd1;
					end
				4'd1: begin
					edge_start <= rowptr[row];
					edge_end <= rowptr[row + 1];
					state <= 4'd2;
				end
				4'd2: begin
					begin : sv2v_autoblock_2
						integer i;
						for (i = 0; i < LANES; i = i + 1)
							accumulator[i] <= 0;
					end
					edge_index <= edge_start;
					state <= 4'd3;
				end
				4'd3: begin
					column <= columns[edge_index];
					weight <= weight_values[edge_index >> 7];
					read_base <= 0;
					state <= 4'd4;
				end
				4'd4: begin
					begin : sv2v_autoblock_3
						integer i;
						for (i = 0; i < LANES; i = i + 1)
							if ((i >= sv2v_cast_32(read_base)) && (i < (sv2v_cast_32(read_base) + READS)))
								value[i] <= feature_values[i];
					end
					if ((sv2v_cast_32(read_base) + READS) >= LANES)
						state <= 4'd5;
					else
						read_base <= read_base + sv2v_cast_5_signed(READS);
				end
				4'd5:
					if (arith_ready)
						state <= 4'd6;
				4'd6:
					if (arith_rsp_valid) begin
						begin : sv2v_autoblock_4
							integer i;
							for (i = 0; i < LANES; i = i + 1)
								product[i] <= arith_result[i * 32+:32];
						end
						state <= 4'd7;
					end
				4'd7:
					if (arith_ready)
						state <= 4'd8;
				4'd8:
					if (arith_rsp_valid) begin
						begin : sv2v_autoblock_5
							integer i;
							for (i = 0; i < LANES; i = i + 1)
								begin
									accumulator[i] <= sum[i];
									if ((edge_index + 1) == edge_end)
										result[i] <= (activate && (sum[i] < 0) ? 32'sd0 : sum[i]);
								end
						end
						if ((edge_index + 1) == edge_end) begin
							emit_lane <= 0;
							state <= 4'd9;
						end
						else begin
							edge_index <= edge_index + 1;
							state <= 4'd3;
						end
					end
				4'd9:
					if (out_ready) begin
						if (emit_lane != 15)
							emit_lane <= emit_lane + 1;
						else if ((channel + LANES) == 128) begin
							channel <= 0;
							if ((row + 1) == {16'd0, rows}) begin
								done <= 1;
								state <= 4'd0;
							end
							else begin
								row <= row + 1;
								state <= 4'd1;
							end
						end
						else begin
							channel <= channel + LANES;
							state <= 4'd2;
						end
					end
				default: state <= 4'd0;
			endcase
		end
	end
endmodule
module GcnBank (
	clk,
	we,
	waddr,
	raddr,
	wdata,
	rdata
);
	input wire clk;
	input wire we;
	input wire [6:0] waddr;
	input wire [6:0] raddr;
	input wire [31:0] wdata;
	output wire [31:0] rdata;
	reg [31:0] words [0:127];
	assign rdata = words[raddr];
	always @(posedge clk)
		if (we)
			words[waddr] <= wdata;
endmodule
module GptNormNative (
	clk,
	reset,
	start,
	fused,
	phase,
	write,
	bank,
	address,
	wdata,
	rdata,
	busy,
	done,
	div_start,
	div_signed,
	div_a,
	div_b,
	div_busy,
	div_done,
	div_result
);
	reg _sv2v_0;
	input wire clk;
	input wire reset;
	input wire start;
	input wire fused;
	input wire [2:0] phase;
	input wire write;
	input wire [1:0] bank;
	input wire [9:0] address;
	input wire signed [31:0] wdata;
	output reg signed [31:0] rdata;
	output reg busy;
	output reg done;
	output wire div_start;
	output wire div_signed;
	output wire [31:0] div_a;
	output wire [31:0] div_b;
	input wire div_busy;
	input wire div_done;
	input wire [31:0] div_result;
	reg signed [31:0] source [0:127];
	reg signed [31:0] gain [0:127];
	reg signed [31:0] offset [0:127];
	reg signed [31:0] result [0:127];
	reg [2:0] stage;
	reg [2:0] step;
	reg automatic_run;
	reg [6:0] index;
	reg signed [31:0] sum;
	reg signed [31:0] mean;
	reg [31:0] squares;
	reg [31:0] variance;
	reg [31:0] std;
	reg [31:0] radicand;
	reg [33:0] remainder;
	reg [15:0] root;
	reg [4:0] iteration;
	reg signed [31:0] sample;
	reg signed [31:0] factor;
	reg signed [31:0] bias;
	reg signed [31:0] delta;
	reg signed [31:0] product;
	reg signed [31:0] affine;
	reg signed [31:0] rounded;
	reg signed [63:0] square;
	reg [33:0] next_rem;
	reg [33:0] trial;
	reg [15:0] next_root;
	assign div_start = (busy && (stage == 5)) && (step == 3);
	assign div_signed = 1'b1;
	assign div_a = product;
	assign div_b = std;
	always @(*) begin
		if (_sv2v_0)
			;
		rdata = result[address[6:0]];
		next_rem = (remainder << 2) | {32'b00000000000000000000000000000000, radicand[31:30]};
		trial = ({18'b000000000000000000, root} << 2) | 34'd1;
		next_root = {root[14:0], next_rem >= trial};
	end
	task automatic advance;
		input reg [2:0] next_stage;
		begin
			index <= 0;
			step <= 0;
			if (automatic_run && (stage != 6))
				stage <= next_stage;
			else begin
				busy <= 0;
				done <= 1;
			end
		end
	endtask
	function automatic signed [63:0] sv2v_cast_64_signed;
		input reg signed [63:0] inp;
		sv2v_cast_64_signed = inp;
	endfunction
	function automatic signed [31:0] sv2v_cast_32_signed;
		input reg signed [31:0] inp;
		sv2v_cast_32_signed = inp;
	endfunction
	always @(posedge clk) begin
		done <= 0;
		if (reset) begin
			busy <= 0;
			stage <= 0;
			index <= 0;
			automatic_run <= 0;
			sum <= 0;
			mean <= 0;
			squares <= 0;
			variance <= 0;
			std <= 0;
			radicand <= 0;
			remainder <= 0;
			root <= 0;
			iteration <= 0;
			step <= 0;
		end
		else if (!busy) begin
			if (write)
				case (bank)
					0: source[address[6:0]] <= wdata;
					1: gain[address[6:0]] <= wdata;
					2: offset[address[6:0]] <= wdata;
					default: result[address[6:0]] <= wdata;
				endcase
			if (start) begin
				busy <= 1;
				automatic_run <= fused;
				stage <= (fused ? 3'd0 : phase);
				step <= 0;
				index <= 0;
				if (fused || (phase == 0))
					sum <= 0;
				if (phase == 2)
					squares <= 0;
				if (phase == 4) begin
					radicand <= (variance << 4) + 4096;
					remainder <= 0;
					root <= 0;
					iteration <= 0;
				end
			end
		end
		else
			case (stage)
				0:
					if (step == 0) begin
						sample <= source[index];
						step <= 1;
					end
					else begin
						sum <= sum + sample;
						step <= 0;
						index <= index + 1'b1;
						if (index == 127)
							advance(1);
					end
				1: begin
					mean <= (sum <<< 6) >>> 7;
					squares <= 0;
					advance(2);
				end
				2:
					case (step)
						0: begin
							sample <= source[index];
							step <= 1;
						end
						1: begin
							delta <= (sample <<< 6) - mean;
							step <= 2;
						end
						2: begin
							square <= sv2v_cast_64_signed(delta) * sv2v_cast_64_signed(delta);
							step <= 3;
						end
						3: begin
							squares <= squares + sv2v_cast_32_signed(square >> 4);
							step <= 0;
							index <= index + 1'b1;
							if (index == 127)
								advance(3);
						end
						default: step <= 0;
					endcase
				3: begin
					variance <= squares >> 7;
					radicand <= ((squares >> 7) << 4) + 4096;
					remainder <= 0;
					root <= 0;
					iteration <= 0;
					advance(4);
				end
				4: begin
					root <= next_root;
					remainder <= (next_rem >= trial ? next_rem - trial : next_rem);
					radicand <= radicand << 2;
					iteration <= iteration + 1'b1;
					if (iteration == 15) begin
						std <= {16'b0000000000000000, next_root};
						advance(5);
					end
				end
				5:
					case (step)
						0: begin
							sample <= source[index];
							factor <= gain[index];
							bias <= offset[index];
							step <= 1;
						end
						1: begin
							delta <= (sample <<< 6) - mean;
							step <= 2;
						end
						2: begin
							product <= delta * factor;
							step <= 3;
						end
						3: step <= 4;
						4:
							if (div_done) begin
								affine <= $signed(div_result) + bias;
								step <= 5;
							end
						5: begin
							result[index] <= affine;
							step <= 0;
							index <= index + 1'b1;
							if (index == 127)
								advance(6);
						end
						default: step <= 0;
					endcase
				6:
					case (step)
						0: begin
							sample <= result[index];
							step <= 1;
						end
						1: begin
							rounded <= (sample < 0 ? -((2 - sample) >>> 2) : (sample + 2) >>> 2);
							step <= 2;
						end
						2: begin
							result[index] <= (rounded > 127 ? 127 : (rounded < -128 ? -128 : rounded));
							step <= 0;
							index <= index + 1'b1;
							if (index == 127)
								advance(0);
						end
						default: step <= 0;
					endcase
				default: begin
					busy <= 0;
					done <= 1;
				end
			endcase
	end
	initial _sv2v_0 = 0;
endmodule
module GptSoftmaxNative (
	clk,
	reset,
	start,
	fused,
	phase,
	row,
	write,
	address,
	wdata,
	rdata,
	busy,
	done,
	div_start,
	div_signed,
	div_a,
	div_b,
	div_busy,
	div_done,
	div_result
);
	input wire clk;
	input wire reset;
	input wire start;
	input wire fused;
	input wire [2:0] phase;
	input wire row;
	input wire write;
	input wire [9:0] address;
	input wire signed [31:0] wdata;
	output wire signed [31:0] rdata;
	output reg busy;
	output reg done;
	output wire div_start;
	output wire div_signed;
	output wire [31:0] div_a;
	output wire [31:0] div_b;
	input wire div_busy;
	input wire div_done;
	input wire [31:0] div_result;
	reg signed [31:0] source [0:1];
	reg signed [31:0] result [0:1];
	reg signed [31:0] maximum;
	reg [15:0] exponent [0:1];
	reg [15:0] exp_value;
	reg [16:0] total;
	reg [16:0] square;
	reg [1:0] stage;
	reg [3:0] step;
	reg index;
	reg limit;
	reg automatic_run;
	reg signed [31:0] difference;
	wire [31:0] halved;
	reg [12:0] distance;
	reg [12:0] correction;
	reg [12:0] base;
	reg [20:0] scaled;
	reg [4:0] shift;
	reg [8:0] polynomial;
	reg [17:0] biased;
	reg [22:0] product;
	reg [22:0] numerator;
	assign rdata = result[address[0]];
	assign halved = difference >>> 32'sd1;
	assign div_start = (busy && (stage == 2)) && (step == 2);
	assign div_signed = 1'b0;
	assign div_a = {9'd0, numerator};
	assign div_b = {15'd0, total};
	task automatic advance;
		input reg [1:0] next_stage;
		begin
			index <= 0;
			step <= 0;
			if (automatic_run && (stage != 2))
				stage <= next_stage;
			else begin
				busy <= 0;
				done <= 1;
			end
		end
	endtask
	function automatic [20:0] sv2v_cast_21;
		input reg [20:0] inp;
		sv2v_cast_21 = inp;
	endfunction
	function automatic [12:0] sv2v_cast_13;
		input reg [12:0] inp;
		sv2v_cast_13 = inp;
	endfunction
	function automatic [8:0] sv2v_cast_9;
		input reg [8:0] inp;
		sv2v_cast_9 = inp;
	endfunction
	function automatic [16:0] sv2v_cast_17;
		input reg [16:0] inp;
		sv2v_cast_17 = inp;
	endfunction
	function automatic [15:0] sv2v_cast_16;
		input reg [15:0] inp;
		sv2v_cast_16 = inp;
	endfunction
	function automatic [22:0] sv2v_cast_23;
		input reg [22:0] inp;
		sv2v_cast_23 = inp;
	endfunction
	always @(posedge clk) begin
		done <= 0;
		if (reset) begin
			busy <= 0;
			stage <= 0;
			step <= 0;
			index <= 0;
			limit <= 0;
			automatic_run <= 0;
			maximum <= 0;
			total <= 0;
		end
		else if (!busy) begin
			if (write)
				source[address[0]] <= wdata;
			if (start) begin
				busy <= 1;
				automatic_run <= fused;
				stage <= (fused ? 2'd0 : phase[1:0]);
				index <= 0;
				step <= 0;
				limit <= row;
				if (fused || (phase == 0))
					maximum <= -33'sd2147483648;
				if (fused || (phase == 1))
					total <= 0;
			end
		end
		else
			case (stage)
				0: begin
					if ((index <= limit) && (source[index] > maximum))
						maximum <= source[index];
					index <= 1;
					if (index)
						advance(1);
				end
				1: begin
					step <= step + 1'b1;
					case (step)
						0: difference <= maximum - source[index];
						1: distance <= (halved > 4096 ? 13'd4096 : halved[12:0]);
						2: scaled <= sv2v_cast_21(distance) * 21'd370;
						3: shift <= scaled[20:16];
						4: correction <= sv2v_cast_13(shift) * 13'd177;
						5: base <= 13'd346 + correction;
						6: polynomial <= sv2v_cast_9(base - distance);
						7: square <= sv2v_cast_17(polynomial) * sv2v_cast_17(polynomial);
						8: biased <= {1'b0, square} + 18'd62885;
						9: exp_value <= sv2v_cast_16(biased >> (shift + 5'd2));
						10: begin
							exponent[index] <= (index <= limit ? exp_value : 16'd0);
							if (index <= limit)
								total <= total + {1'b0, exp_value};
							step <= 0;
							index <= 1;
							if (index)
								advance(2);
						end
						default: step <= 0;
					endcase
				end
				2:
					case (step)
						0: begin
							product <= sv2v_cast_23(exponent[index]) * 23'd127;
							step <= 1;
						end
						1: begin
							numerator <= product + {7'd0, total[16:1]};
							step <= 2;
						end
						2: step <= 3;
						3:
							if (div_done) begin
								result[index] <= div_result;
								step <= 0;
								index <= 1;
								if (index)
									advance(0);
							end
						default: step <= 0;
					endcase
				default: begin
					busy <= 0;
					done <= 1;
				end
			endcase
	end
endmodule
module GptVectorNative (
	clk,
	reset,
	start,
	operation,
	count,
	divisor,
	write,
	bank,
	address,
	wdata,
	rdata,
	busy,
	done,
	div_start,
	div_signed,
	div_a,
	div_b,
	div_busy,
	div_done,
	div_result
);
	input wire clk;
	input wire reset;
	input wire start;
	input wire [1:0] operation;
	input wire [10:0] count;
	input wire [31:0] divisor;
	input wire write;
	input wire bank;
	input wire [9:0] address;
	input wire signed [31:0] wdata;
	output wire signed [31:0] rdata;
	output reg busy;
	output reg done;
	output wire div_start;
	output wire div_signed;
	output wire [31:0] div_a;
	output wire [31:0] div_b;
	input wire div_busy;
	input wire div_done;
	input wire [31:0] div_result;
	reg [4:0] state;
	wire signed [31:0] source_data;
	wire signed [31:0] other_data;
	reg [9:0] index;
	reg [10:0] size;
	reg [1:0] op;
	reg [31:0] scale;
	reg [31:0] numerator;
	reg [31:0] round_magnitude;
	reg signed [31:0] value;
	reg signed [31:0] second;
	reg signed [31:0] q;
	reg signed [31:0] magnitude;
	reg signed [31:0] delta;
	reg signed [31:0] coefficient;
	reg signed [31:0] square;
	reg signed [31:0] correction;
	reg signed [31:0] gate;
	reg signed [31:0] product;
	reg signed [31:0] biased;
	reg signed [31:0] activation;
	reg negative;
	assign div_start = state == 5'd19;
	assign div_signed = 1'b0;
	assign div_a = numerator;
	assign div_b = scale;
	GptVectorMemory source_store(
		.clk(clk),
		.we(((!reset && (state == 5'd0)) && write) && !bank),
		.waddr(address),
		.wdata(wdata),
		.raddr(index),
		.rdata(source_data)
	);
	GptVectorMemory other_store(
		.clk(clk),
		.we(((!reset && (state == 5'd0)) && write) && bank),
		.waddr(address),
		.wdata(wdata),
		.raddr(index),
		.rdata(other_data)
	);
	GptVectorMemory result_store(
		.clk(clk),
		.we(!reset && (state == 5'd22)),
		.waddr(index),
		.wdata(value),
		.raddr(address),
		.rdata(rdata)
	);
	always @(posedge clk) begin
		done <= 0;
		if (reset) begin
			busy <= 0;
			state <= 5'd0;
		end
		else
			case (state)
				5'd0:
					if (start) begin
						busy <= 1;
						index <= 0;
						size <= count;
						op <= operation;
						scale <= divisor;
						state <= 5'd1;
					end
				5'd1: begin
					value <= source_data;
					second <= other_data;
					state <= 5'd2;
				end
				5'd2:
					case (op)
						0: begin
							q <= value <<< 2;
							state <= 5'd3;
						end
						1: begin
							negative <= value[31];
							state <= 5'd17;
						end
						2: begin
							value <= value + second;
							state <= 5'd21;
						end
						default: state <= 5'd21;
					endcase
				5'd3: begin
					magnitude <= (q < 0 ? -q : q);
					state <= 5'd4;
				end
				5'd4: begin
					delta <= (magnitude > 160 ? 160 : magnitude) - 160;
					state <= 5'd5;
				end
				5'd5: begin
					coefficient <= 591 * delta;
					state <= 5'd6;
				end
				5'd6: begin
					square <= coefficient * delta;
					state <= 5'd7;
				end
				5'd7: begin
					correction <= square >>> 12;
					state <= 5'd8;
				end
				5'd8: begin
					gate <= (q < 0 ? correction : 8192 - correction);
					state <= 5'd9;
				end
				5'd9: begin
					product <= magnitude * gate;
					state <= 5'd10;
				end
				5'd10: begin
					biased <= product + 4096;
					state <= 5'd11;
				end
				5'd11: begin
					activation <= biased >>> 13;
					state <= 5'd12;
				end
				5'd12: begin
					value <= (q < 0 ? -activation : activation);
					state <= 5'd13;
				end
				5'd13: begin
					negative <= value[31];
					round_magnitude <= (value < 0 ? -value : value);
					state <= 5'd14;
				end
				5'd14: begin
					round_magnitude <= round_magnitude + 2;
					state <= 5'd15;
				end
				5'd15: begin
					round_magnitude <= round_magnitude >> 2;
					state <= 5'd16;
				end
				5'd16: begin
					value <= (negative ? -$signed(round_magnitude) : $signed(round_magnitude));
					state <= 5'd21;
				end
				5'd17: begin
					numerator <= (value < 0 ? -value : value);
					state <= 5'd18;
				end
				5'd18: begin
					numerator <= numerator + (scale >> 1);
					state <= 5'd19;
				end
				5'd19: state <= 5'd20;
				5'd20:
					if (div_done) begin
						value <= (negative ? -$signed(div_result) : $signed(div_result));
						state <= 5'd21;
					end
				5'd21: begin
					value <= (value > 127 ? 127 : (value < -128 ? -128 : value));
					state <= 5'd22;
				end
				5'd22: begin
					index <= index + 1'b1;
					state <= 5'd1;
					if (({1'b0, index} + 11'd1) == size) begin
						busy <= 0;
						done <= 1;
						state <= 5'd0;
					end
				end
				default: state <= 5'd0;
			endcase
	end
endmodule
module GptVectorBank (
	clk,
	we,
	waddr,
	raddr,
	wdata,
	rdata
);
	input wire clk;
	input wire we;
	input wire [6:0] waddr;
	input wire [6:0] raddr;
	input wire [31:0] wdata;
	output wire [31:0] rdata;
	reg [31:0] words [0:127];
	assign rdata = words[raddr];
	always @(posedge clk)
		if (we)
			words[waddr] <= wdata;
endmodule
module GptVectorMemory (
	clk,
	we,
	waddr,
	raddr,
	wdata,
	rdata
);
	input wire clk;
	input wire we;
	input wire [9:0] waddr;
	input wire [9:0] raddr;
	input wire [31:0] wdata;
	output wire [31:0] rdata;
	wire [31:0] values [0:7];
	genvar _gv_i_4;
	function automatic signed [2:0] sv2v_cast_3_signed;
		input reg signed [2:0] inp;
		sv2v_cast_3_signed = inp;
	endfunction
	generate
		for (_gv_i_4 = 0; _gv_i_4 < 8; _gv_i_4 = _gv_i_4 + 1) begin : banks
			localparam i = _gv_i_4;
			GptVectorBank memory(
				.clk(clk),
				.we(we && (waddr[9:7] == sv2v_cast_3_signed(i))),
				.waddr(waddr[6:0]),
				.wdata(wdata),
				.raddr(raddr[6:0]),
				.rdata(values[i])
			);
		end
	endgenerate
	assign rdata = values[raddr[9:7]];
endmodule
