module ExpandedSad (
	clk,
	reset,
	native_valid,
	native_ready,
	native_last,
	native_mask,
	native_a,
	native_b,
	native_rsp_valid,
	native_rsp_ready,
	native_result,
	native_busy,
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
	reg _sv2v_0;
	input wire clk;
	input wire reset;
	input wire native_valid;
	output wire native_ready;
	input wire native_last;
	input wire [15:0] native_mask;
	input wire [511:0] native_a;
	input wire [511:0] native_b;
	output wire native_rsp_valid;
	input wire native_rsp_ready;
	output wire [31:0] native_result;
	output wire native_busy;
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
	reg [3:0] state;
	reg bank_valid;
	wire bank_ready;
	wire bank_rsp_valid;
	reg bank_rsp_ready;
	wire bank_busy;
	reg [15:0] bank_mask;
	reg [15:0] active;
	reg [79:0] bank_operation;
	reg [511:0] bank_a;
	reg [511:0] bank_b;
	reg [511:0] bank_c;
	reg [511:0] bank_d;
	wire [511:0] bank_result;
	reg [511:0] values;
	reg [31:0] accumulator;
	reg [1:0] round;
	reg last_block;
	wire native_accept;
	wire primitive_access;
	assign primitive_access = state == 4'd0;
	assign cmd_ready = primitive_access && bank_ready;
	assign rsp_valid = primitive_access && bank_rsp_valid;
	assign result = bank_result;
	assign native_ready = bank_ready && (((state == 4'd0) && !cmd_valid) || (state == 4'd9));
	assign native_accept = native_valid && native_ready;
	assign native_busy = state != 4'd0;
	assign native_rsp_valid = state == 4'd8;
	assign native_result = accumulator;
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
		.native_start(1'b0),
		.native_signed(1'b0),
		.native_a(32'd0),
		.native_b(32'd0),
		.native_busy(),
		.native_done(),
		.native_result()
	);
	always @(*) begin
		if (_sv2v_0)
			;
		bank_valid = 0;
		bank_mask = 0;
		bank_operation = 0;
		bank_a = 0;
		bank_b = 0;
		bank_c = 0;
		bank_d = 0;
		bank_rsp_ready = 0;
		case (state)
			4'd0, 4'd9: begin
				bank_valid = native_valid;
				bank_mask = native_mask;
				bank_operation = {16 {5'd1}};
				bank_a = native_b;
				bank_b = native_a;
				if (primitive_access) begin
					bank_rsp_ready = rsp_ready;
					if (cmd_valid) begin
						bank_valid = cmd_valid;
						bank_mask = mask;
						bank_operation = operation;
						bank_a = a;
						bank_b = b;
						bank_c = c;
						bank_d = d;
					end
				end
			end
			4'd1, 4'd3, 4'd5, 4'd7: bank_rsp_ready = 1;
			4'd2: begin
				bank_valid = 1;
				bank_mask = 16'hffff;
				begin : sv2v_autoblock_1
					integer i;
					for (i = 0; i < 16; i = i + 1)
						begin
							bank_operation[i * 5+:5] = (values[(i * 32) + 31] ? 5'd1 : 5'd21);
							bank_a[i * 32+:32] = (values[(i * 32) + 31] ? 32'd0 : values[i * 32+:32]);
							bank_b[i * 32+:32] = values[i * 32+:32];
						end
				end
			end
			4'd4: begin
				bank_valid = 1;
				case (round)
					0: bank_mask = 16'h00ff;
					1: bank_mask = 16'h000f;
					2: bank_mask = 16'h0003;
					3: bank_mask = 16'h0001;
				endcase
				begin : sv2v_autoblock_2
					integer i;
					for (i = 0; i < 8; i = i + 1)
						begin
							bank_a[i * 32+:32] = values[i * 64+:32];
							bank_b[i * 32+:32] = values[(i * 64) + 32+:32];
						end
				end
			end
			4'd6: begin
				bank_valid = 1;
				bank_mask = 16'h0001;
				bank_a[31:0] = accumulator;
				bank_b[31:0] = values[31:0];
			end
			default:
				;
		endcase
	end
	always @(posedge clk)
		if (reset) begin
			state <= 4'd0;
			active <= 0;
			values <= 0;
			accumulator <= 0;
			round <= 0;
			last_block <= 0;
		end
		else
			case (state)
				4'd0, 4'd9:
					if (native_accept) begin
						if (state == 4'd0)
							accumulator <= 0;
						active <= native_mask;
						last_block <= native_last;
						state <= 4'd1;
					end
				4'd1:
					if (bank_rsp_valid) begin
						begin : sv2v_autoblock_3
							integer i;
							for (i = 0; i < 16; i = i + 1)
								values[i * 32+:32] <= (active[i] ? bank_result[i * 32+:32] : 32'd0);
						end
						state <= 4'd2;
					end
				4'd2:
					if (bank_ready)
						state <= 4'd3;
				4'd3:
					if (bank_rsp_valid) begin
						values <= bank_result;
						round <= 0;
						state <= 4'd4;
					end
				4'd4:
					if (bank_ready)
						state <= 4'd5;
				4'd5:
					if (bank_rsp_valid) begin
						values <= bank_result;
						round <= round + 1'b1;
						state <= (round == 3 ? 4'd6 : 4'd4);
					end
				4'd6:
					if (bank_ready)
						state <= 4'd7;
				4'd7:
					if (bank_rsp_valid) begin
						accumulator <= bank_result[31:0];
						state <= (last_block ? 4'd8 : 4'd9);
					end
				4'd8:
					if (native_rsp_ready)
						state <= 4'd0;
				default: state <= 4'd0;
			endcase
	initial _sv2v_0 = 0;
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
