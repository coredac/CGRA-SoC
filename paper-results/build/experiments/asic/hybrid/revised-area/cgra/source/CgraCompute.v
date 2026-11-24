module Mux__d8b1aea79052d3bb (
	clk,
	in_,
	out,
	reset,
	sel
);
	reg _sv2v_0;
	input wire [0:0] clk;
	input wire [403:0] in_;
	output reg [201:0] out;
	input wire [0:0] reset;
	input wire [0:0] sel;
	always @(*) begin : up_mux
		if (_sv2v_0)
			;
		out = in_[(1 - sel) * 202+:202];
	end
	initial _sv2v_0 = 0;
endmodule
module BypassQueue1EntryRTL__bf52db5a0197a1a1 (
	clk,
	count,
	reset,
	recv__msg,
	recv__rdy,
	recv__val,
	send__msg,
	send__rdy,
	send__val
);
	reg _sv2v_0;
	input wire [0:0] clk;
	output wire [0:0] count;
	input wire [0:0] reset;
	input wire [201:0] recv__msg;
	output reg [0:0] recv__rdy;
	input wire [0:0] recv__val;
	output wire [201:0] send__msg;
	input wire [0:0] send__rdy;
	output reg [0:0] send__val;
	reg [201:0] entry;
	reg [0:0] full;
	wire [0:0] bypass_mux__clk;
	wire [403:0] bypass_mux__in_;
	wire [201:0] bypass_mux__out;
	wire [0:0] bypass_mux__reset;
	wire [0:0] bypass_mux__sel;
	Mux__d8b1aea79052d3bb bypass_mux(
		.clk(bypass_mux__clk),
		.in_(bypass_mux__in_),
		.out(bypass_mux__out),
		.reset(bypass_mux__reset),
		.sel(bypass_mux__sel)
	);
	always @(*) begin : _lambda__s_cgra_bypass_queue_q_recv_rdy
		if (_sv2v_0)
			;
		recv__rdy = ~full;
	end
	always @(*) begin : _lambda__s_cgra_bypass_queue_q_send_val
		if (_sv2v_0)
			;
		send__val = full | recv__val;
	end
	always @(posedge clk) begin : ff_bypass1
		if (reset)
			full <= 1'd0;
		else
			full <= ~send__rdy & (full | recv__val);
		if ((~send__rdy & ~full) & recv__val)
			entry <= recv__msg;
	end
	assign bypass_mux__clk = clk;
	assign bypass_mux__reset = reset;
	assign bypass_mux__in_[202+:202] = recv__msg;
	assign bypass_mux__in_[0+:202] = entry;
	assign send__msg = bypass_mux__out;
	assign bypass_mux__sel = full;
	assign count = full;
	initial _sv2v_0 = 0;
endmodule
module BypassQueueRTL__de7a0309a59fe496 (
	clk,
	count,
	reset,
	recv__msg,
	recv__rdy,
	recv__val,
	send__msg,
	send__rdy,
	send__val
);
	input wire [0:0] clk;
	output wire [0:0] count;
	input wire [0:0] reset;
	input wire [201:0] recv__msg;
	output wire [0:0] recv__rdy;
	input wire [0:0] recv__val;
	output wire [201:0] send__msg;
	input wire [0:0] send__rdy;
	output wire [0:0] send__val;
	wire [0:0] q__clk;
	wire [0:0] q__count;
	wire [0:0] q__reset;
	wire [201:0] q__recv__msg;
	wire [0:0] q__recv__rdy;
	wire [0:0] q__recv__val;
	wire [201:0] q__send__msg;
	wire [0:0] q__send__rdy;
	wire [0:0] q__send__val;
	BypassQueue1EntryRTL__bf52db5a0197a1a1 q(
		.clk(q__clk),
		.count(q__count),
		.reset(q__reset),
		.recv__msg(q__recv__msg),
		.recv__rdy(q__recv__rdy),
		.recv__val(q__recv__val),
		.send__msg(q__send__msg),
		.send__rdy(q__send__rdy),
		.send__val(q__send__val)
	);
	assign q__clk = clk;
	assign q__reset = reset;
	assign q__recv__msg = recv__msg;
	assign recv__rdy = q__recv__rdy;
	assign q__recv__val = recv__val;
	assign send__msg = q__send__msg;
	assign q__send__rdy = send__rdy;
	assign send__val = q__send__val;
	assign count = q__count;
endmodule
module NormalQueueCtrlRTL__num_entries_2 (
	clk,
	count,
	raddr,
	recv_rdy,
	recv_val,
	reset,
	send_rdy,
	send_val,
	waddr,
	wen
);
	reg _sv2v_0;
	input wire [0:0] clk;
	output reg [1:0] count;
	output wire [0:0] raddr;
	output reg [0:0] recv_rdy;
	input wire [0:0] recv_val;
	input wire [0:0] reset;
	input wire [0:0] send_rdy;
	output reg [0:0] send_val;
	output wire [0:0] waddr;
	output wire [0:0] wen;
	localparam [1:0] __const__num_entries_at__lambda__s_cgra_controller_crossbar_input_units_0__queue_ctrl_recv_rdy = 2'd2;
	localparam [1:0] __const__num_entries_at_up_reg = 2'd2;
	reg [0:0] head;
	reg [0:0] recv_xfer;
	reg [0:0] send_xfer;
	reg [0:0] tail;
	always @(*) begin : _lambda__s_cgra_controller_crossbar_input_units_0__queue_ctrl_recv_rdy
		if (_sv2v_0)
			;
		recv_rdy = count < __const__num_entries_at__lambda__s_cgra_controller_crossbar_input_units_0__queue_ctrl_recv_rdy;
	end
	always @(*) begin : _lambda__s_cgra_controller_crossbar_input_units_0__queue_ctrl_recv_xfer
		if (_sv2v_0)
			;
		recv_xfer = recv_val & recv_rdy;
	end
	always @(*) begin : _lambda__s_cgra_controller_crossbar_input_units_0__queue_ctrl_send_val
		if (_sv2v_0)
			;
		send_val = count > 2'd0;
	end
	always @(*) begin : _lambda__s_cgra_controller_crossbar_input_units_0__queue_ctrl_send_xfer
		if (_sv2v_0)
			;
		send_xfer = send_val & send_rdy;
	end
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(posedge clk) begin : up_reg
		if (reset) begin
			head <= 1'd0;
			tail <= 1'd0;
			count <= 2'd0;
		end
		else begin
			if (recv_xfer)
				tail <= (tail < (sv2v_cast_1(__const__num_entries_at_up_reg) - 1'd1) ? tail + 1'd1 : 1'd0);
			if (send_xfer)
				head <= (head < (sv2v_cast_1(__const__num_entries_at_up_reg) - 1'd1) ? head + 1'd1 : 1'd0);
			if (recv_xfer & ~send_xfer)
				count <= count + 2'd1;
			else if (~recv_xfer & send_xfer)
				count <= count - 2'd1;
		end
	end
	assign wen = recv_xfer;
	assign waddr = tail;
	assign raddr = head;
	initial _sv2v_0 = 0;
endmodule
module RegisterFile__d463a2c62cc05ca9 (
	clk,
	raddr,
	rdata,
	reset,
	waddr,
	wdata,
	wen
);
	reg _sv2v_0;
	input wire [0:0] clk;
	input wire [0:0] raddr;
	output reg [202:0] rdata;
	input wire [0:0] reset;
	input wire [0:0] waddr;
	input wire [202:0] wdata;
	input wire [0:0] wen;
	localparam [0:0] __const__rd_ports_at_up_rf_read = 1'd1;
	localparam [0:0] __const__wr_ports_at_up_rf_write = 1'd1;
	reg [202:0] regs [0:1];
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : up_rf_read
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__rd_ports_at_up_rf_read; i = i + 1'd1)
				rdata[sv2v_cast_1(i) * 203+:203] = regs[raddr[sv2v_cast_1(i)+:1]];
		end
	end
	always @(posedge clk) begin : up_rf_write
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__wr_ports_at_up_rf_write; i = i + 1'd1)
				if (wen[sv2v_cast_1(i)+:1])
					regs[waddr[sv2v_cast_1(i)+:1]] <= wdata[sv2v_cast_1(i) * 203+:203];
		end
	end
	initial _sv2v_0 = 0;
endmodule
module NormalQueueDpathRTL__073e940336a17504 (
	clk,
	raddr,
	recv_msg,
	reset,
	send_msg,
	waddr,
	wen
);
	input wire [0:0] clk;
	input wire [0:0] raddr;
	input wire [202:0] recv_msg;
	input wire [0:0] reset;
	output wire [202:0] send_msg;
	input wire [0:0] waddr;
	input wire [0:0] wen;
	wire [0:0] rf__clk;
	wire [0:0] rf__raddr;
	wire [202:0] rf__rdata;
	wire [0:0] rf__reset;
	wire [0:0] rf__waddr;
	wire [202:0] rf__wdata;
	wire [0:0] rf__wen;
	RegisterFile__d463a2c62cc05ca9 rf(
		.clk(rf__clk),
		.raddr(rf__raddr),
		.rdata(rf__rdata),
		.reset(rf__reset),
		.waddr(rf__waddr),
		.wdata(rf__wdata),
		.wen(rf__wen)
	);
	assign rf__clk = clk;
	assign rf__reset = reset;
	assign rf__raddr[0+:1] = raddr;
	assign send_msg = rf__rdata[0+:203];
	assign rf__wen[0+:1] = wen;
	assign rf__waddr[0+:1] = waddr;
	assign rf__wdata[0+:203] = recv_msg;
endmodule
module NormalQueueRTL__073e940336a17504 (
	clk,
	count,
	reset,
	recv__msg,
	recv__rdy,
	recv__val,
	send__msg,
	send__rdy,
	send__val
);
	input wire [0:0] clk;
	output wire [1:0] count;
	input wire [0:0] reset;
	input wire [202:0] recv__msg;
	output wire [0:0] recv__rdy;
	input wire [0:0] recv__val;
	output wire [202:0] send__msg;
	input wire [0:0] send__rdy;
	output wire [0:0] send__val;
	wire [0:0] ctrl__clk;
	wire [1:0] ctrl__count;
	wire [0:0] ctrl__raddr;
	wire [0:0] ctrl__recv_rdy;
	wire [0:0] ctrl__recv_val;
	wire [0:0] ctrl__reset;
	wire [0:0] ctrl__send_rdy;
	wire [0:0] ctrl__send_val;
	wire [0:0] ctrl__waddr;
	wire [0:0] ctrl__wen;
	NormalQueueCtrlRTL__num_entries_2 ctrl(
		.clk(ctrl__clk),
		.count(ctrl__count),
		.raddr(ctrl__raddr),
		.recv_rdy(ctrl__recv_rdy),
		.recv_val(ctrl__recv_val),
		.reset(ctrl__reset),
		.send_rdy(ctrl__send_rdy),
		.send_val(ctrl__send_val),
		.waddr(ctrl__waddr),
		.wen(ctrl__wen)
	);
	wire [0:0] dpath__clk;
	wire [0:0] dpath__raddr;
	wire [202:0] dpath__recv_msg;
	wire [0:0] dpath__reset;
	wire [202:0] dpath__send_msg;
	wire [0:0] dpath__waddr;
	wire [0:0] dpath__wen;
	NormalQueueDpathRTL__073e940336a17504 dpath(
		.clk(dpath__clk),
		.raddr(dpath__raddr),
		.recv_msg(dpath__recv_msg),
		.reset(dpath__reset),
		.send_msg(dpath__send_msg),
		.waddr(dpath__waddr),
		.wen(dpath__wen)
	);
	assign ctrl__clk = clk;
	assign ctrl__reset = reset;
	assign dpath__clk = clk;
	assign dpath__reset = reset;
	assign dpath__wen = ctrl__wen;
	assign dpath__waddr = ctrl__waddr;
	assign dpath__raddr = ctrl__raddr;
	assign ctrl__recv_val = recv__val;
	assign recv__rdy = ctrl__recv_rdy;
	assign dpath__recv_msg = recv__msg;
	assign send__val = ctrl__send_val;
	assign ctrl__send_rdy = send__rdy;
	assign send__msg = dpath__send_msg;
	assign count = ctrl__count;
endmodule
module InputUnitRTL__a3b50730ebafae08 (
	clk,
	reset,
	recv__msg,
	recv__rdy,
	recv__val,
	send__msg,
	send__rdy,
	send__val
);
	input wire [0:0] clk;
	input wire [0:0] reset;
	input wire [202:0] recv__msg;
	output wire [0:0] recv__rdy;
	input wire [0:0] recv__val;
	output wire [202:0] send__msg;
	input wire [0:0] send__rdy;
	output wire [0:0] send__val;
	wire [0:0] queue__clk;
	wire [1:0] queue__count;
	wire [0:0] queue__reset;
	wire [202:0] queue__recv__msg;
	wire [0:0] queue__recv__rdy;
	wire [0:0] queue__recv__val;
	wire [202:0] queue__send__msg;
	wire [0:0] queue__send__rdy;
	wire [0:0] queue__send__val;
	NormalQueueRTL__073e940336a17504 queue(
		.clk(queue__clk),
		.count(queue__count),
		.reset(queue__reset),
		.recv__msg(queue__recv__msg),
		.recv__rdy(queue__recv__rdy),
		.recv__val(queue__recv__val),
		.send__msg(queue__send__msg),
		.send__rdy(queue__send__rdy),
		.send__val(queue__send__val)
	);
	assign queue__clk = clk;
	assign queue__reset = reset;
	assign queue__recv__msg = recv__msg;
	assign recv__rdy = queue__recv__rdy;
	assign queue__recv__val = recv__val;
	assign send__msg = queue__send__msg;
	assign queue__send__rdy = send__rdy;
	assign send__val = queue__send__val;
endmodule
module OutputUnitRTL__2420395e72f34cd9 (
	clk,
	reset,
	recv__msg,
	recv__rdy,
	recv__val,
	send__msg,
	send__rdy,
	send__val
);
	input wire [0:0] clk;
	input wire [0:0] reset;
	input wire [202:0] recv__msg;
	output wire [0:0] recv__rdy;
	input wire [0:0] recv__val;
	output wire [202:0] send__msg;
	input wire [0:0] send__rdy;
	output wire [0:0] send__val;
	assign send__msg = recv__msg;
	assign recv__rdy = send__rdy;
	assign send__val = recv__val;
endmodule
module XbarRouteUnitRTL__319eb850679e11ae (
	clk,
	reset,
	recv__msg,
	recv__rdy,
	recv__val,
	send__msg,
	send__rdy,
	send__val
);
	reg _sv2v_0;
	input wire [0:0] clk;
	input wire [0:0] reset;
	input wire [202:0] recv__msg;
	output reg [0:0] recv__rdy;
	input wire [0:0] recv__val;
	output wire [202:0] send__msg;
	input wire [0:0] send__rdy;
	output reg [0:0] send__val;
	localparam [0:0] __const__num_outports_at_up_ru_routing = 1'd1;
	reg [0:0] out_dir;
	wire [0:0] send_val;
	always @(*) begin : up_ru_recv_rdy
		if (_sv2v_0)
			;
		recv__rdy = send__rdy[out_dir+:1] > 1'd0;
	end
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : up_ru_routing
		if (_sv2v_0)
			;
		out_dir = recv__msg[202];
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_up_ru_routing; i = i + 1'd1)
				send__val[sv2v_cast_1(i)+:1] = 1'd0;
		end
		if (recv__val)
			send__val[out_dir+:1] = 1'd1;
	end
	assign send__msg[0+:203] = recv__msg;
	assign send_val[0:0] = send__val[0+:1];
	initial _sv2v_0 = 0;
endmodule
module RegEnRst__Type_Bits5__reset_value_1 (
	clk,
	en,
	in_,
	out,
	reset
);
	input wire [0:0] clk;
	input wire [0:0] en;
	input wire [4:0] in_;
	output reg [4:0] out;
	input wire [0:0] reset;
	localparam [0:0] __const__reset_value_at_up_regenrst = 1'd1;
	function automatic [4:0] sv2v_cast_5;
		input reg [4:0] inp;
		sv2v_cast_5 = inp;
	endfunction
	always @(posedge clk) begin : up_regenrst
		if (reset)
			out <= sv2v_cast_5(__const__reset_value_at_up_regenrst);
		else if (en)
			out <= in_;
	end
endmodule
module RoundRobinArbiterEn__nreqs_5 (
	clk,
	en,
	grants,
	reqs,
	reset
);
	reg _sv2v_0;
	input wire [0:0] clk;
	input wire [0:0] en;
	output reg [4:0] grants;
	input wire [4:0] reqs;
	input wire [0:0] reset;
	localparam [2:0] __const__nreqs_at_comb_reqs_int = 3'd5;
	localparam [3:0] __const__nreqsX2_at_comb_reqs_int = 4'd10;
	localparam [2:0] __const__nreqs_at_comb_grants = 3'd5;
	localparam [2:0] __const__nreqs_at_comb_priority_int = 3'd5;
	localparam [3:0] __const__nreqsX2_at_comb_priority_int = 4'd10;
	localparam [3:0] __const__nreqsX2_at_comb_kills = 4'd10;
	localparam [3:0] __const__nreqsX2_at_comb_grants_int = 4'd10;
	reg [9:0] grants_int;
	reg [10:0] kills;
	reg [0:0] priority_en;
	reg [9:0] priority_int;
	reg [9:0] reqs_int;
	wire [0:0] priority_reg__clk;
	wire [0:0] priority_reg__en;
	wire [4:0] priority_reg__in_;
	wire [4:0] priority_reg__out;
	wire [0:0] priority_reg__reset;
	RegEnRst__Type_Bits5__reset_value_1 priority_reg(
		.clk(priority_reg__clk),
		.en(priority_reg__en),
		.in_(priority_reg__in_),
		.out(priority_reg__out),
		.reset(priority_reg__reset)
	);
	function automatic [2:0] sv2v_cast_3;
		input reg [2:0] inp;
		sv2v_cast_3 = inp;
	endfunction
	function automatic [3:0] sv2v_cast_4;
		input reg [3:0] inp;
		sv2v_cast_4 = inp;
	endfunction
	always @(*) begin : comb_grants
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__nreqs_at_comb_grants; i = i + 1'd1)
				grants[sv2v_cast_3(i)] = grants_int[sv2v_cast_4(i)] | grants_int[sv2v_cast_4(__const__nreqs_at_comb_grants) + sv2v_cast_4(i)];
		end
	end
	always @(*) begin : comb_grants_int
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__nreqsX2_at_comb_grants_int; i = i + 1'd1)
				if (priority_int[sv2v_cast_4(i)])
					grants_int[sv2v_cast_4(i)] = reqs_int[sv2v_cast_4(i)];
				else
					grants_int[sv2v_cast_4(i)] = ~kills[sv2v_cast_4(i)] & reqs_int[sv2v_cast_4(i)];
		end
	end
	always @(*) begin : comb_kills
		if (_sv2v_0)
			;
		kills[4'd0] = 1'd1;
		begin : sv2v_autoblock_3
			reg [31:0] i;
			for (i = 1'd0; i < __const__nreqsX2_at_comb_kills; i = i + 1'd1)
				if (priority_int[sv2v_cast_4(i)])
					kills[sv2v_cast_4(i) + 4'd1] = reqs_int[sv2v_cast_4(i)];
				else
					kills[sv2v_cast_4(i) + 4'd1] = kills[sv2v_cast_4(i)] | (~kills[sv2v_cast_4(i)] & reqs_int[sv2v_cast_4(i)]);
		end
	end
	always @(*) begin : comb_priority_en
		if (_sv2v_0)
			;
		priority_en = (grants != 5'd0) & en;
	end
	always @(*) begin : comb_priority_int
		if (_sv2v_0)
			;
		priority_int[4'd4:4'd0] = priority_reg__out;
		priority_int[4'd9:sv2v_cast_4(__const__nreqs_at_comb_priority_int)] = 5'd0;
	end
	always @(*) begin : comb_reqs_int
		if (_sv2v_0)
			;
		reqs_int[4'd4:4'd0] = reqs;
		reqs_int[4'd9:sv2v_cast_4(__const__nreqs_at_comb_reqs_int)] = reqs;
	end
	assign priority_reg__clk = clk;
	assign priority_reg__reset = reset;
	assign priority_reg__en = priority_en;
	assign priority_reg__in_[4:1] = grants[3:0];
	assign priority_reg__in_[0:0] = grants[4:4];
	initial _sv2v_0 = 0;
endmodule
module Encoder__in_nbits_5__out_nbits_3 (
	clk,
	in_,
	out,
	reset
);
	reg _sv2v_0;
	input wire [0:0] clk;
	input wire [4:0] in_;
	output reg [2:0] out;
	input wire [0:0] reset;
	function automatic [2:0] sv2v_cast_3;
		input reg [2:0] inp;
		sv2v_cast_3 = inp;
	endfunction
	always @(*) begin : encode
		if (_sv2v_0)
			;
		out = 3'd0;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < 3'd5; i = i + 1'd1)
				if (in_[sv2v_cast_3(i)])
					out = sv2v_cast_3(i);
		end
	end
	initial _sv2v_0 = 0;
endmodule
module Mux__b4a48e54e0753d2b (
	clk,
	in_,
	out,
	reset,
	sel
);
	reg _sv2v_0;
	input wire [0:0] clk;
	input wire [1014:0] in_;
	output reg [202:0] out;
	input wire [0:0] reset;
	input wire [2:0] sel;
	always @(*) begin : up_mux
		if (_sv2v_0)
			;
		out = in_[(4 - sel) * 203+:203];
	end
	initial _sv2v_0 = 0;
endmodule
module SwitchUnitRTL__8a9c0fce71882b68 (
	clk,
	reset,
	recv__msg,
	recv__rdy,
	recv__val,
	send__msg,
	send__rdy,
	send__val
);
	reg _sv2v_0;
	input wire [0:0] clk;
	input wire [0:0] reset;
	input wire [1014:0] recv__msg;
	output reg [4:0] recv__rdy;
	input wire [4:0] recv__val;
	output wire [202:0] send__msg;
	input wire [0:0] send__rdy;
	output reg [0:0] send__val;
	localparam [2:0] __const__num_inports_at_up_get_en = 3'd5;
	wire [0:0] arbiter__clk;
	wire [0:0] arbiter__en;
	wire [4:0] arbiter__grants;
	wire [4:0] arbiter__reqs;
	wire [0:0] arbiter__reset;
	RoundRobinArbiterEn__nreqs_5 arbiter(
		.clk(arbiter__clk),
		.en(arbiter__en),
		.grants(arbiter__grants),
		.reqs(arbiter__reqs),
		.reset(arbiter__reset)
	);
	wire [0:0] encoder__clk;
	wire [4:0] encoder__in_;
	wire [2:0] encoder__out;
	wire [0:0] encoder__reset;
	Encoder__in_nbits_5__out_nbits_3 encoder(
		.clk(encoder__clk),
		.in_(encoder__in_),
		.out(encoder__out),
		.reset(encoder__reset)
	);
	wire [0:0] mux__clk;
	wire [1014:0] mux__in_;
	wire [202:0] mux__out;
	wire [0:0] mux__reset;
	wire [2:0] mux__sel;
	Mux__b4a48e54e0753d2b mux(
		.clk(mux__clk),
		.in_(mux__in_),
		.out(mux__out),
		.reset(mux__reset),
		.sel(mux__sel)
	);
	function automatic [2:0] sv2v_cast_3;
		input reg [2:0] inp;
		sv2v_cast_3 = inp;
	endfunction
	always @(*) begin : up_get_en
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_up_get_en; i = i + 1'd1)
				recv__rdy[4 - sv2v_cast_3(i)+:1] = send__rdy & (mux__sel == sv2v_cast_3(i));
		end
	end
	always @(*) begin : up_send_val
		if (_sv2v_0)
			;
		send__val = arbiter__grants > 5'd0;
	end
	assign arbiter__clk = clk;
	assign arbiter__reset = reset;
	assign arbiter__en = 1'd1;
	assign mux__clk = clk;
	assign mux__reset = reset;
	assign send__msg = mux__out;
	assign encoder__clk = clk;
	assign encoder__reset = reset;
	assign encoder__in_ = arbiter__grants;
	assign mux__sel = encoder__out;
	assign arbiter__reqs[0:0] = recv__val[4+:1];
	assign mux__in_[812+:203] = recv__msg[812+:203];
	assign arbiter__reqs[1:1] = recv__val[3+:1];
	assign mux__in_[609+:203] = recv__msg[609+:203];
	assign arbiter__reqs[2:2] = recv__val[2+:1];
	assign mux__in_[406+:203] = recv__msg[406+:203];
	assign arbiter__reqs[3:3] = recv__val[1+:1];
	assign mux__in_[203+:203] = recv__msg[203+:203];
	assign arbiter__reqs[4:4] = recv__val[0+:1];
	assign mux__in_[0+:203] = recv__msg[0+:203];
	initial _sv2v_0 = 0;
endmodule
module XbarRTL__e29f75d9969a53ec (
	clk,
	reset,
	recv__msg,
	recv__rdy,
	recv__val,
	send__msg,
	send__rdy,
	send__val
);
	input wire [0:0] clk;
	input wire [0:0] reset;
	input wire [1014:0] recv__msg;
	output wire [4:0] recv__rdy;
	input wire [4:0] recv__val;
	output wire [202:0] send__msg;
	input wire [0:0] send__rdy;
	output wire [0:0] send__val;
	wire [0:0] input_units__clk [0:4];
	wire [0:0] input_units__reset [0:4];
	wire [202:0] input_units__recv__msg [0:4];
	wire [0:0] input_units__recv__rdy [0:4];
	wire [0:0] input_units__recv__val [0:4];
	wire [202:0] input_units__send__msg [0:4];
	wire [0:0] input_units__send__rdy [0:4];
	wire [0:0] input_units__send__val [0:4];
	InputUnitRTL__a3b50730ebafae08 input_units__0(
		.clk(input_units__clk[0]),
		.reset(input_units__reset[0]),
		.recv__msg(input_units__recv__msg[0]),
		.recv__rdy(input_units__recv__rdy[0]),
		.recv__val(input_units__recv__val[0]),
		.send__msg(input_units__send__msg[0]),
		.send__rdy(input_units__send__rdy[0]),
		.send__val(input_units__send__val[0])
	);
	InputUnitRTL__a3b50730ebafae08 input_units__1(
		.clk(input_units__clk[1]),
		.reset(input_units__reset[1]),
		.recv__msg(input_units__recv__msg[1]),
		.recv__rdy(input_units__recv__rdy[1]),
		.recv__val(input_units__recv__val[1]),
		.send__msg(input_units__send__msg[1]),
		.send__rdy(input_units__send__rdy[1]),
		.send__val(input_units__send__val[1])
	);
	InputUnitRTL__a3b50730ebafae08 input_units__2(
		.clk(input_units__clk[2]),
		.reset(input_units__reset[2]),
		.recv__msg(input_units__recv__msg[2]),
		.recv__rdy(input_units__recv__rdy[2]),
		.recv__val(input_units__recv__val[2]),
		.send__msg(input_units__send__msg[2]),
		.send__rdy(input_units__send__rdy[2]),
		.send__val(input_units__send__val[2])
	);
	InputUnitRTL__a3b50730ebafae08 input_units__3(
		.clk(input_units__clk[3]),
		.reset(input_units__reset[3]),
		.recv__msg(input_units__recv__msg[3]),
		.recv__rdy(input_units__recv__rdy[3]),
		.recv__val(input_units__recv__val[3]),
		.send__msg(input_units__send__msg[3]),
		.send__rdy(input_units__send__rdy[3]),
		.send__val(input_units__send__val[3])
	);
	InputUnitRTL__a3b50730ebafae08 input_units__4(
		.clk(input_units__clk[4]),
		.reset(input_units__reset[4]),
		.recv__msg(input_units__recv__msg[4]),
		.recv__rdy(input_units__recv__rdy[4]),
		.recv__val(input_units__recv__val[4]),
		.send__msg(input_units__send__msg[4]),
		.send__rdy(input_units__send__rdy[4]),
		.send__val(input_units__send__val[4])
	);
	wire [0:0] output_units__clk [0:0];
	wire [0:0] output_units__reset [0:0];
	wire [202:0] output_units__recv__msg [0:0];
	wire [0:0] output_units__recv__rdy [0:0];
	wire [0:0] output_units__recv__val [0:0];
	wire [202:0] output_units__send__msg [0:0];
	wire [0:0] output_units__send__rdy [0:0];
	wire [0:0] output_units__send__val [0:0];
	OutputUnitRTL__2420395e72f34cd9 output_units__0(
		.clk(output_units__clk[0]),
		.reset(output_units__reset[0]),
		.recv__msg(output_units__recv__msg[0]),
		.recv__rdy(output_units__recv__rdy[0]),
		.recv__val(output_units__recv__val[0]),
		.send__msg(output_units__send__msg[0]),
		.send__rdy(output_units__send__rdy[0]),
		.send__val(output_units__send__val[0])
	);
	wire [0:0] route_units__clk [0:4];
	wire [0:0] route_units__reset [0:4];
	wire [202:0] route_units__recv__msg [0:4];
	wire [0:0] route_units__recv__rdy [0:4];
	wire [0:0] route_units__recv__val [0:4];
	wire [202:0] route_units__send__msg [0:4];
	wire [0:0] route_units__send__rdy [0:4];
	wire [0:0] route_units__send__val [0:4];
	XbarRouteUnitRTL__319eb850679e11ae route_units__0(
		.clk(route_units__clk[0]),
		.reset(route_units__reset[0]),
		.recv__msg(route_units__recv__msg[0]),
		.recv__rdy(route_units__recv__rdy[0]),
		.recv__val(route_units__recv__val[0]),
		.send__msg(route_units__send__msg[0]),
		.send__rdy(route_units__send__rdy[0]),
		.send__val(route_units__send__val[0])
	);
	XbarRouteUnitRTL__319eb850679e11ae route_units__1(
		.clk(route_units__clk[1]),
		.reset(route_units__reset[1]),
		.recv__msg(route_units__recv__msg[1]),
		.recv__rdy(route_units__recv__rdy[1]),
		.recv__val(route_units__recv__val[1]),
		.send__msg(route_units__send__msg[1]),
		.send__rdy(route_units__send__rdy[1]),
		.send__val(route_units__send__val[1])
	);
	XbarRouteUnitRTL__319eb850679e11ae route_units__2(
		.clk(route_units__clk[2]),
		.reset(route_units__reset[2]),
		.recv__msg(route_units__recv__msg[2]),
		.recv__rdy(route_units__recv__rdy[2]),
		.recv__val(route_units__recv__val[2]),
		.send__msg(route_units__send__msg[2]),
		.send__rdy(route_units__send__rdy[2]),
		.send__val(route_units__send__val[2])
	);
	XbarRouteUnitRTL__319eb850679e11ae route_units__3(
		.clk(route_units__clk[3]),
		.reset(route_units__reset[3]),
		.recv__msg(route_units__recv__msg[3]),
		.recv__rdy(route_units__recv__rdy[3]),
		.recv__val(route_units__recv__val[3]),
		.send__msg(route_units__send__msg[3]),
		.send__rdy(route_units__send__rdy[3]),
		.send__val(route_units__send__val[3])
	);
	XbarRouteUnitRTL__319eb850679e11ae route_units__4(
		.clk(route_units__clk[4]),
		.reset(route_units__reset[4]),
		.recv__msg(route_units__recv__msg[4]),
		.recv__rdy(route_units__recv__rdy[4]),
		.recv__val(route_units__recv__val[4]),
		.send__msg(route_units__send__msg[4]),
		.send__rdy(route_units__send__rdy[4]),
		.send__val(route_units__send__val[4])
	);
	wire [0:0] switch_units__clk [0:0];
	wire [0:0] switch_units__reset [0:0];
	wire [1014:0] switch_units__recv__msg [0:0];
	wire [4:0] switch_units__recv__rdy [0:0];
	wire [4:0] switch_units__recv__val [0:0];
	wire [202:0] switch_units__send__msg [0:0];
	wire [0:0] switch_units__send__rdy [0:0];
	wire [0:0] switch_units__send__val [0:0];
	SwitchUnitRTL__8a9c0fce71882b68 switch_units__0(
		.clk(switch_units__clk[0]),
		.reset(switch_units__reset[0]),
		.recv__msg(switch_units__recv__msg[0]),
		.recv__rdy(switch_units__recv__rdy[0]),
		.recv__val(switch_units__recv__val[0]),
		.send__msg(switch_units__send__msg[0]),
		.send__rdy(switch_units__send__rdy[0]),
		.send__val(switch_units__send__val[0])
	);
	assign input_units__clk[0] = clk;
	assign input_units__reset[0] = reset;
	assign input_units__clk[1] = clk;
	assign input_units__reset[1] = reset;
	assign input_units__clk[2] = clk;
	assign input_units__reset[2] = reset;
	assign input_units__clk[3] = clk;
	assign input_units__reset[3] = reset;
	assign input_units__clk[4] = clk;
	assign input_units__reset[4] = reset;
	assign route_units__clk[0] = clk;
	assign route_units__reset[0] = reset;
	assign route_units__clk[1] = clk;
	assign route_units__reset[1] = reset;
	assign route_units__clk[2] = clk;
	assign route_units__reset[2] = reset;
	assign route_units__clk[3] = clk;
	assign route_units__reset[3] = reset;
	assign route_units__clk[4] = clk;
	assign route_units__reset[4] = reset;
	assign switch_units__clk[0] = clk;
	assign switch_units__reset[0] = reset;
	assign output_units__clk[0] = clk;
	assign output_units__reset[0] = reset;
	assign input_units__recv__msg[0] = recv__msg[812+:203];
	assign recv__rdy[4+:1] = input_units__recv__rdy[0];
	assign input_units__recv__val[0] = recv__val[4+:1];
	assign route_units__recv__msg[0] = input_units__send__msg[0];
	assign input_units__send__rdy[0] = route_units__recv__rdy[0];
	assign route_units__recv__val[0] = input_units__send__val[0];
	assign input_units__recv__msg[1] = recv__msg[609+:203];
	assign recv__rdy[3+:1] = input_units__recv__rdy[1];
	assign input_units__recv__val[1] = recv__val[3+:1];
	assign route_units__recv__msg[1] = input_units__send__msg[1];
	assign input_units__send__rdy[1] = route_units__recv__rdy[1];
	assign route_units__recv__val[1] = input_units__send__val[1];
	assign input_units__recv__msg[2] = recv__msg[406+:203];
	assign recv__rdy[2+:1] = input_units__recv__rdy[2];
	assign input_units__recv__val[2] = recv__val[2+:1];
	assign route_units__recv__msg[2] = input_units__send__msg[2];
	assign input_units__send__rdy[2] = route_units__recv__rdy[2];
	assign route_units__recv__val[2] = input_units__send__val[2];
	assign input_units__recv__msg[3] = recv__msg[203+:203];
	assign recv__rdy[1+:1] = input_units__recv__rdy[3];
	assign input_units__recv__val[3] = recv__val[1+:1];
	assign route_units__recv__msg[3] = input_units__send__msg[3];
	assign input_units__send__rdy[3] = route_units__recv__rdy[3];
	assign route_units__recv__val[3] = input_units__send__val[3];
	assign input_units__recv__msg[4] = recv__msg[0+:203];
	assign recv__rdy[0+:1] = input_units__recv__rdy[4];
	assign input_units__recv__val[4] = recv__val[0+:1];
	assign route_units__recv__msg[4] = input_units__send__msg[4];
	assign input_units__send__rdy[4] = route_units__recv__rdy[4];
	assign route_units__recv__val[4] = input_units__send__val[4];
	assign switch_units__recv__msg[0][812+:203] = route_units__send__msg[0][0+:203];
	assign route_units__send__rdy[0][0+:1] = switch_units__recv__rdy[0][4+:1];
	assign switch_units__recv__val[0][4+:1] = route_units__send__val[0][0+:1];
	assign switch_units__recv__msg[0][609+:203] = route_units__send__msg[1][0+:203];
	assign route_units__send__rdy[1][0+:1] = switch_units__recv__rdy[0][3+:1];
	assign switch_units__recv__val[0][3+:1] = route_units__send__val[1][0+:1];
	assign switch_units__recv__msg[0][406+:203] = route_units__send__msg[2][0+:203];
	assign route_units__send__rdy[2][0+:1] = switch_units__recv__rdy[0][2+:1];
	assign switch_units__recv__val[0][2+:1] = route_units__send__val[2][0+:1];
	assign switch_units__recv__msg[0][203+:203] = route_units__send__msg[3][0+:203];
	assign route_units__send__rdy[3][0+:1] = switch_units__recv__rdy[0][1+:1];
	assign switch_units__recv__val[0][1+:1] = route_units__send__val[3][0+:1];
	assign switch_units__recv__msg[0][0+:203] = route_units__send__msg[4][0+:203];
	assign route_units__send__rdy[4][0+:1] = switch_units__recv__rdy[0][0+:1];
	assign switch_units__recv__val[0][0+:1] = route_units__send__val[4][0+:1];
	assign output_units__recv__msg[0] = switch_units__send__msg[0];
	assign switch_units__send__rdy[0] = output_units__recv__rdy[0];
	assign output_units__recv__val[0] = switch_units__send__val[0];
	assign send__msg[0+:203] = output_units__send__msg[0];
	assign output_units__send__rdy[0] = send__rdy[0+:1];
	assign send__val[0+:1] = output_units__send__val[0];
endmodule
module RegisterFile__701216ea6bbc5838 (
	clk,
	raddr,
	rdata,
	reset,
	waddr,
	wdata,
	wen
);
	reg _sv2v_0;
	input wire [0:0] clk;
	input wire [0:0] raddr;
	output reg [197:0] rdata;
	input wire [0:0] reset;
	input wire [0:0] waddr;
	input wire [197:0] wdata;
	input wire [0:0] wen;
	localparam [0:0] __const__rd_ports_at_up_rf_read = 1'd1;
	localparam [0:0] __const__wr_ports_at_up_rf_write = 1'd1;
	reg [197:0] regs [0:1];
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : up_rf_read
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__rd_ports_at_up_rf_read; i = i + 1'd1)
				rdata[sv2v_cast_1(i) * 198+:198] = regs[raddr[sv2v_cast_1(i)+:1]];
		end
	end
	always @(posedge clk) begin : up_rf_write
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__wr_ports_at_up_rf_write; i = i + 1'd1)
				if (wen[sv2v_cast_1(i)+:1])
					regs[waddr[sv2v_cast_1(i)+:1]] <= wdata[sv2v_cast_1(i) * 198+:198];
		end
	end
	initial _sv2v_0 = 0;
endmodule
module NormalQueueDpathRTL__3f50370143e38621 (
	clk,
	raddr,
	recv_msg,
	reset,
	send_msg,
	waddr,
	wen
);
	input wire [0:0] clk;
	input wire [0:0] raddr;
	input wire [197:0] recv_msg;
	input wire [0:0] reset;
	output wire [197:0] send_msg;
	input wire [0:0] waddr;
	input wire [0:0] wen;
	wire [0:0] rf__clk;
	wire [0:0] rf__raddr;
	wire [197:0] rf__rdata;
	wire [0:0] rf__reset;
	wire [0:0] rf__waddr;
	wire [197:0] rf__wdata;
	wire [0:0] rf__wen;
	RegisterFile__701216ea6bbc5838 rf(
		.clk(rf__clk),
		.raddr(rf__raddr),
		.rdata(rf__rdata),
		.reset(rf__reset),
		.waddr(rf__waddr),
		.wdata(rf__wdata),
		.wen(rf__wen)
	);
	assign rf__clk = clk;
	assign rf__reset = reset;
	assign rf__raddr[0+:1] = raddr;
	assign send_msg = rf__rdata[0+:198];
	assign rf__wen[0+:1] = wen;
	assign rf__waddr[0+:1] = waddr;
	assign rf__wdata[0+:198] = recv_msg;
endmodule
module NormalQueueRTL__3f50370143e38621 (
	clk,
	count,
	reset,
	recv__msg,
	recv__rdy,
	recv__val,
	send__msg,
	send__rdy,
	send__val
);
	input wire [0:0] clk;
	output wire [1:0] count;
	input wire [0:0] reset;
	input wire [197:0] recv__msg;
	output wire [0:0] recv__rdy;
	input wire [0:0] recv__val;
	output wire [197:0] send__msg;
	input wire [0:0] send__rdy;
	output wire [0:0] send__val;
	wire [0:0] ctrl__clk;
	wire [1:0] ctrl__count;
	wire [0:0] ctrl__raddr;
	wire [0:0] ctrl__recv_rdy;
	wire [0:0] ctrl__recv_val;
	wire [0:0] ctrl__reset;
	wire [0:0] ctrl__send_rdy;
	wire [0:0] ctrl__send_val;
	wire [0:0] ctrl__waddr;
	wire [0:0] ctrl__wen;
	NormalQueueCtrlRTL__num_entries_2 ctrl(
		.clk(ctrl__clk),
		.count(ctrl__count),
		.raddr(ctrl__raddr),
		.recv_rdy(ctrl__recv_rdy),
		.recv_val(ctrl__recv_val),
		.reset(ctrl__reset),
		.send_rdy(ctrl__send_rdy),
		.send_val(ctrl__send_val),
		.waddr(ctrl__waddr),
		.wen(ctrl__wen)
	);
	wire [0:0] dpath__clk;
	wire [0:0] dpath__raddr;
	wire [197:0] dpath__recv_msg;
	wire [0:0] dpath__reset;
	wire [197:0] dpath__send_msg;
	wire [0:0] dpath__waddr;
	wire [0:0] dpath__wen;
	NormalQueueDpathRTL__3f50370143e38621 dpath(
		.clk(dpath__clk),
		.raddr(dpath__raddr),
		.recv_msg(dpath__recv_msg),
		.reset(dpath__reset),
		.send_msg(dpath__send_msg),
		.waddr(dpath__waddr),
		.wen(dpath__wen)
	);
	assign ctrl__clk = clk;
	assign ctrl__reset = reset;
	assign dpath__clk = clk;
	assign dpath__reset = reset;
	assign dpath__wen = ctrl__wen;
	assign dpath__waddr = ctrl__waddr;
	assign dpath__raddr = ctrl__raddr;
	assign ctrl__recv_val = recv__val;
	assign recv__rdy = ctrl__recv_rdy;
	assign dpath__recv_msg = recv__msg;
	assign send__val = ctrl__send_val;
	assign ctrl__send_rdy = send__rdy;
	assign send__msg = dpath__send_msg;
	assign count = ctrl__count;
endmodule
module RegisterFile__dbd6e049392bb272 (
	clk,
	raddr,
	rdata,
	reset,
	waddr,
	wdata,
	wen
);
	reg _sv2v_0;
	input wire [0:0] clk;
	input wire [0:0] raddr;
	output reg [201:0] rdata;
	input wire [0:0] reset;
	input wire [0:0] waddr;
	input wire [201:0] wdata;
	input wire [0:0] wen;
	localparam [0:0] __const__rd_ports_at_up_rf_read = 1'd1;
	localparam [0:0] __const__wr_ports_at_up_rf_write = 1'd1;
	reg [201:0] regs [0:1];
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : up_rf_read
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__rd_ports_at_up_rf_read; i = i + 1'd1)
				rdata[sv2v_cast_1(i) * 202+:202] = regs[raddr[sv2v_cast_1(i)+:1]];
		end
	end
	always @(posedge clk) begin : up_rf_write
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__wr_ports_at_up_rf_write; i = i + 1'd1)
				if (wen[sv2v_cast_1(i)+:1])
					regs[waddr[sv2v_cast_1(i)+:1]] <= wdata[sv2v_cast_1(i) * 202+:202];
		end
	end
	initial _sv2v_0 = 0;
endmodule
module NormalQueueDpathRTL__06dcb483b017b133 (
	clk,
	raddr,
	recv_msg,
	reset,
	send_msg,
	waddr,
	wen
);
	input wire [0:0] clk;
	input wire [0:0] raddr;
	input wire [201:0] recv_msg;
	input wire [0:0] reset;
	output wire [201:0] send_msg;
	input wire [0:0] waddr;
	input wire [0:0] wen;
	wire [0:0] rf__clk;
	wire [0:0] rf__raddr;
	wire [201:0] rf__rdata;
	wire [0:0] rf__reset;
	wire [0:0] rf__waddr;
	wire [201:0] rf__wdata;
	wire [0:0] rf__wen;
	RegisterFile__dbd6e049392bb272 rf(
		.clk(rf__clk),
		.raddr(rf__raddr),
		.rdata(rf__rdata),
		.reset(rf__reset),
		.waddr(rf__waddr),
		.wdata(rf__wdata),
		.wen(rf__wen)
	);
	assign rf__clk = clk;
	assign rf__reset = reset;
	assign rf__raddr[0+:1] = raddr;
	assign send_msg = rf__rdata[0+:202];
	assign rf__wen[0+:1] = wen;
	assign rf__waddr[0+:1] = waddr;
	assign rf__wdata[0+:202] = recv_msg;
endmodule
module NormalQueueRTL__06dcb483b017b133 (
	clk,
	count,
	reset,
	recv__msg,
	recv__rdy,
	recv__val,
	send__msg,
	send__rdy,
	send__val
);
	input wire [0:0] clk;
	output wire [1:0] count;
	input wire [0:0] reset;
	input wire [201:0] recv__msg;
	output wire [0:0] recv__rdy;
	input wire [0:0] recv__val;
	output wire [201:0] send__msg;
	input wire [0:0] send__rdy;
	output wire [0:0] send__val;
	wire [0:0] ctrl__clk;
	wire [1:0] ctrl__count;
	wire [0:0] ctrl__raddr;
	wire [0:0] ctrl__recv_rdy;
	wire [0:0] ctrl__recv_val;
	wire [0:0] ctrl__reset;
	wire [0:0] ctrl__send_rdy;
	wire [0:0] ctrl__send_val;
	wire [0:0] ctrl__waddr;
	wire [0:0] ctrl__wen;
	NormalQueueCtrlRTL__num_entries_2 ctrl(
		.clk(ctrl__clk),
		.count(ctrl__count),
		.raddr(ctrl__raddr),
		.recv_rdy(ctrl__recv_rdy),
		.recv_val(ctrl__recv_val),
		.reset(ctrl__reset),
		.send_rdy(ctrl__send_rdy),
		.send_val(ctrl__send_val),
		.waddr(ctrl__waddr),
		.wen(ctrl__wen)
	);
	wire [0:0] dpath__clk;
	wire [0:0] dpath__raddr;
	wire [201:0] dpath__recv_msg;
	wire [0:0] dpath__reset;
	wire [201:0] dpath__send_msg;
	wire [0:0] dpath__waddr;
	wire [0:0] dpath__wen;
	NormalQueueDpathRTL__06dcb483b017b133 dpath(
		.clk(dpath__clk),
		.raddr(dpath__raddr),
		.recv_msg(dpath__recv_msg),
		.reset(dpath__reset),
		.send_msg(dpath__send_msg),
		.waddr(dpath__waddr),
		.wen(dpath__wen)
	);
	assign ctrl__clk = clk;
	assign ctrl__reset = reset;
	assign dpath__clk = clk;
	assign dpath__reset = reset;
	assign dpath__wen = ctrl__wen;
	assign dpath__waddr = ctrl__waddr;
	assign dpath__raddr = ctrl__raddr;
	assign ctrl__recv_val = recv__val;
	assign recv__rdy = ctrl__recv_rdy;
	assign dpath__recv_msg = recv__msg;
	assign send__val = ctrl__send_val;
	assign ctrl__send_rdy = send__rdy;
	assign send__msg = dpath__send_msg;
	assign count = ctrl__count;
endmodule
module ChannelRTL__14ea05451db65d6a (
	clk,
	reset,
	recv__msg,
	recv__rdy,
	recv__val,
	send__msg,
	send__rdy,
	send__val
);
	input wire [0:0] clk;
	input wire [0:0] reset;
	input wire [201:0] recv__msg;
	output wire [0:0] recv__rdy;
	input wire [0:0] recv__val;
	output wire [201:0] send__msg;
	input wire [0:0] send__rdy;
	output wire [0:0] send__val;
	wire [0:0] queues__clk [0:0];
	wire [1:0] queues__count [0:0];
	wire [0:0] queues__reset [0:0];
	wire [201:0] queues__recv__msg [0:0];
	wire [0:0] queues__recv__rdy [0:0];
	wire [0:0] queues__recv__val [0:0];
	wire [201:0] queues__send__msg [0:0];
	wire [0:0] queues__send__rdy [0:0];
	wire [0:0] queues__send__val [0:0];
	NormalQueueRTL__06dcb483b017b133 queues__0(
		.clk(queues__clk[0]),
		.count(queues__count[0]),
		.reset(queues__reset[0]),
		.recv__msg(queues__recv__msg[0]),
		.recv__rdy(queues__recv__rdy[0]),
		.recv__val(queues__recv__val[0]),
		.send__msg(queues__send__msg[0]),
		.send__rdy(queues__send__rdy[0]),
		.send__val(queues__send__val[0])
	);
	assign queues__clk[0] = clk;
	assign queues__reset[0] = reset;
	assign queues__recv__msg[0] = recv__msg;
	assign recv__rdy = queues__recv__rdy[0];
	assign queues__recv__val[0] = recv__val;
	assign send__msg = queues__send__msg[0];
	assign queues__send__rdy[0] = send__rdy;
	assign send__val = queues__send__val[0];
endmodule
module ControllerRTL__d74885762e9d302f (
	cgra_id,
	clk,
	reset,
	dma_cmd__msg,
	dma_cmd__rdy,
	dma_cmd__val,
	dma_done__msg,
	dma_done__rdy,
	dma_done__val,
	recv_from_cpu_pkt__msg,
	recv_from_cpu_pkt__rdy,
	recv_from_cpu_pkt__val,
	recv_from_ctrl_ring_pkt__msg,
	recv_from_ctrl_ring_pkt__rdy,
	recv_from_ctrl_ring_pkt__val,
	recv_from_dma_spm_rd_req__msg,
	recv_from_dma_spm_rd_req__rdy,
	recv_from_dma_spm_rd_req__val,
	recv_from_dma_spm_wr_req__msg,
	recv_from_dma_spm_wr_req__rdy,
	recv_from_dma_spm_wr_req__val,
	recv_from_im2col_pkt__msg,
	recv_from_im2col_pkt__rdy,
	recv_from_im2col_pkt__val,
	recv_from_inter_cgra_noc__msg,
	recv_from_inter_cgra_noc__rdy,
	recv_from_inter_cgra_noc__val,
	recv_from_sram_load_response__msg,
	recv_from_sram_load_response__rdy,
	recv_from_sram_load_response__val,
	recv_from_tile_load_request_pkt__msg,
	recv_from_tile_load_request_pkt__rdy,
	recv_from_tile_load_request_pkt__val,
	recv_from_tile_load_response_pkt__msg,
	recv_from_tile_load_response_pkt__rdy,
	recv_from_tile_load_response_pkt__val,
	recv_from_tile_store_request_pkt__msg,
	recv_from_tile_store_request_pkt__rdy,
	recv_from_tile_store_request_pkt__val,
	send_to_cpu_pkt__msg,
	send_to_cpu_pkt__rdy,
	send_to_cpu_pkt__val,
	send_to_ctrl_ring_pkt__msg,
	send_to_ctrl_ring_pkt__rdy,
	send_to_ctrl_ring_pkt__val,
	send_to_dma_spm_rd_resp__msg,
	send_to_dma_spm_rd_resp__rdy,
	send_to_dma_spm_rd_resp__val,
	send_to_im2col_engine_pkt__msg,
	send_to_im2col_engine_pkt__rdy,
	send_to_im2col_engine_pkt__val,
	send_to_inter_cgra_noc__msg,
	send_to_inter_cgra_noc__rdy,
	send_to_inter_cgra_noc__val,
	send_to_sram_load_request_from_dma__msg,
	send_to_sram_load_request_from_dma__rdy,
	send_to_sram_load_request_from_dma__val,
	send_to_sram_load_request_from_noc__msg,
	send_to_sram_load_request_from_noc__rdy,
	send_to_sram_load_request_from_noc__val,
	send_to_sram_store_request_from_dma__msg,
	send_to_sram_store_request_from_dma__rdy,
	send_to_sram_store_request_from_dma__val,
	send_to_sram_store_request_from_noc__msg,
	send_to_sram_store_request_from_noc__rdy,
	send_to_sram_store_request_from_noc__val,
	send_to_tile_load_response__msg,
	send_to_tile_load_response__rdy,
	send_to_tile_load_response__val
);
	reg _sv2v_0;
	input wire [0:0] cgra_id;
	input wire [0:0] clk;
	input wire [0:0] reset;
	output reg [138:0] dma_cmd__msg;
	input wire [0:0] dma_cmd__rdy;
	output reg [0:0] dma_cmd__val;
	input wire [7:0] dma_done__msg;
	output reg [0:0] dma_done__rdy;
	input wire [0:0] dma_done__val;
	input wire [197:0] recv_from_cpu_pkt__msg;
	output wire [0:0] recv_from_cpu_pkt__rdy;
	input wire [0:0] recv_from_cpu_pkt__val;
	input wire [197:0] recv_from_ctrl_ring_pkt__msg;
	output reg [0:0] recv_from_ctrl_ring_pkt__rdy;
	input wire [0:0] recv_from_ctrl_ring_pkt__val;
	input wire [31:0] recv_from_dma_spm_rd_req__msg;
	output reg [0:0] recv_from_dma_spm_rd_req__rdy;
	input wire [0:0] recv_from_dma_spm_rd_req__val;
	input wire [67:0] recv_from_dma_spm_wr_req__msg;
	output reg [0:0] recv_from_dma_spm_wr_req__rdy;
	input wire [0:0] recv_from_dma_spm_wr_req__val;
	input wire [197:0] recv_from_im2col_pkt__msg;
	output wire [0:0] recv_from_im2col_pkt__rdy;
	input wire [0:0] recv_from_im2col_pkt__val;
	input wire [201:0] recv_from_inter_cgra_noc__msg;
	output reg [0:0] recv_from_inter_cgra_noc__rdy;
	input wire [0:0] recv_from_inter_cgra_noc__val;
	input wire [31:0] recv_from_sram_load_response__msg;
	output reg [0:0] recv_from_sram_load_response__rdy;
	input wire [0:0] recv_from_sram_load_response__val;
	input wire [201:0] recv_from_tile_load_request_pkt__msg;
	output wire [0:0] recv_from_tile_load_request_pkt__rdy;
	input wire [0:0] recv_from_tile_load_request_pkt__val;
	input wire [201:0] recv_from_tile_load_response_pkt__msg;
	output wire [0:0] recv_from_tile_load_response_pkt__rdy;
	input wire [0:0] recv_from_tile_load_response_pkt__val;
	input wire [201:0] recv_from_tile_store_request_pkt__msg;
	output wire [0:0] recv_from_tile_store_request_pkt__rdy;
	input wire [0:0] recv_from_tile_store_request_pkt__val;
	output wire [197:0] send_to_cpu_pkt__msg;
	input wire [0:0] send_to_cpu_pkt__rdy;
	output wire [0:0] send_to_cpu_pkt__val;
	output reg [197:0] send_to_ctrl_ring_pkt__msg;
	input wire [0:0] send_to_ctrl_ring_pkt__rdy;
	output reg [0:0] send_to_ctrl_ring_pkt__val;
	output reg [31:0] send_to_dma_spm_rd_resp__msg;
	input wire [0:0] send_to_dma_spm_rd_resp__rdy;
	output reg [0:0] send_to_dma_spm_rd_resp__val;
	output reg [197:0] send_to_im2col_engine_pkt__msg;
	input wire [0:0] send_to_im2col_engine_pkt__rdy;
	output reg [0:0] send_to_im2col_engine_pkt__val;
	output reg [201:0] send_to_inter_cgra_noc__msg;
	input wire [0:0] send_to_inter_cgra_noc__rdy;
	output reg [0:0] send_to_inter_cgra_noc__val;
	output reg [31:0] send_to_sram_load_request_from_dma__msg;
	input wire [0:0] send_to_sram_load_request_from_dma__rdy;
	output reg [0:0] send_to_sram_load_request_from_dma__val;
	output wire [201:0] send_to_sram_load_request_from_noc__msg;
	input wire [0:0] send_to_sram_load_request_from_noc__rdy;
	output wire [0:0] send_to_sram_load_request_from_noc__val;
	output reg [67:0] send_to_sram_store_request_from_dma__msg;
	input wire [0:0] send_to_sram_store_request_from_dma__rdy;
	output reg [0:0] send_to_sram_store_request_from_dma__val;
	output wire [201:0] send_to_sram_store_request_from_noc__msg;
	input wire [0:0] send_to_sram_store_request_from_noc__rdy;
	output wire [0:0] send_to_sram_store_request_from_noc__val;
	output wire [201:0] send_to_tile_load_response__msg;
	input wire [0:0] send_to_tile_load_response__rdy;
	output wire [0:0] send_to_tile_load_response__val;
	localparam [0:0] __const__has_dma_ports_at_update_dma_cmd_regs = 1'd1;
	localparam [5:0] __const__CMD_DMA_CONFIG_DRAM_ADDR_LO = 6'd44;
	localparam [5:0] __const__CMD_DMA_CONFIG_DRAM_ADDR_HI = 6'd45;
	localparam [5:0] __const__CMD_DMA_CONFIG_SPM_ADDR = 6'd46;
	localparam [5:0] __const__CMD_DMA_CONFIG_BYTES = 6'd47;
	localparam [5:0] __const__CMD_DMA_CONFIG_TAG = 6'd48;
	localparam [0:0] __const__has_dma_ports_at_update_dma_spm_forwarding = 1'd1;
	localparam [0:0] __const__DMA_MVIN = 1'd0;
	localparam [1:0] __const__kFromCpuCtrlAndDataIdx = 2'd3;
	localparam [2:0] __const__kFromInterTileRingIdx = 3'd4;
	localparam [0:0] __const__kLoadRequestInportIdx = 1'd0;
	localparam [1:0] __const__kStoreRequestInportIdx = 2'd2;
	localparam [0:0] __const__kLoadResponseInportIdx = 1'd1;
	localparam [0:0] __const__has_dma_ports_at_update_received_msg = 1'd1;
	localparam [5:0] __const__CMD_DMA_MVIN = 6'd49;
	localparam [5:0] __const__CMD_DMA_MVOUT = 6'd50;
	localparam [0:0] __const__DMA_MVOUT = 1'd1;
	localparam [0:0] __const__has_im2col_engine_at_update_received_msg = 1'd0;
	localparam [5:0] __const__CMD_IM2COL_LAUNCH = 6'd52;
	localparam [5:0] __const__CMD_IM2COL_H = 6'd53;
	localparam [5:0] __const__CMD_IM2COL_W = 6'd54;
	localparam [5:0] __const__CMD_IM2COL_KH = 6'd55;
	localparam [5:0] __const__CMD_IM2COL_KW = 6'd56;
	localparam [5:0] __const__CMD_IM2COL_LOG2_STRIDE = 6'd57;
	localparam [5:0] __const__CMD_IM2COL_DST_SRAM_BASE = 6'd58;
	localparam [4:0] __const__num_tiles_at_update_received_msg = 5'd16;
	localparam [3:0] __const__CMD_LOAD_REQUEST = 4'd10;
	localparam [3:0] __const__CMD_STORE_REQUEST = 4'd12;
	localparam [3:0] __const__CMD_LOAD_RESPONSE = 4'd11;
	localparam [3:0] __const__CMD_COMPLETE = 4'd14;
	localparam [4:0] __const__CMD_LEAF_COUNTER_COMPLETE = 5'd31;
	localparam [0:0] __const__has_global_reduce_at_update_received_msg = 1'd0;
	localparam [4:0] __const__CMD_GLOBAL_REDUCE_ADD = 5'd18;
	localparam [4:0] __const__CMD_GLOBAL_REDUCE_COUNT = 5'd17;
	localparam [1:0] __const__CMD_CONFIG = 2'd3;
	localparam [2:0] __const__CMD_CONFIG_PROLOGUE_FU = 3'd4;
	localparam [2:0] __const__CMD_CONFIG_PROLOGUE_FU_CROSSBAR = 3'd5;
	localparam [2:0] __const__CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR = 3'd6;
	localparam [2:0] __const__CMD_CONFIG_TOTAL_CTRL_COUNT = 3'd7;
	localparam [3:0] __const__CMD_CONFIG_COUNT_PER_ITER = 4'd8;
	localparam [3:0] __const__CMD_CONFIG_CTRL_LOWER_BOUND = 4'd9;
	localparam [3:0] __const__CMD_CONST = 4'd13;
	localparam [4:0] __const__CMD_GLOBAL_REDUCE_ADD_RESPONSE = 5'd20;
	localparam [4:0] __const__CMD_GLOBAL_REDUCE_MUL_RESPONSE = 5'd21;
	localparam [4:0] __const__CMD_CONFIG_STREAMING_LD_START_ADDR = 5'd23;
	localparam [4:0] __const__CMD_CONFIG_STREAMING_LD_STRIDE = 5'd24;
	localparam [4:0] __const__CMD_CONFIG_STREAMING_LD_END_ADDR = 5'd25;
	localparam [0:0] __const__CMD_PAUSE = 1'd1;
	localparam [4:0] __const__CMD_PRESERVE = 5'd22;
	localparam [3:0] __const__CMD_RESUME = 4'd15;
	localparam [4:0] __const__CMD_RECORD_PHI_ADDR = 5'd16;
	localparam [1:0] __const__CMD_TERMINATE = 2'd2;
	localparam [5:0] __const__CMD_REARM = 6'd59;
	localparam [0:0] __const__CMD_LAUNCH = 1'd0;
	localparam [4:0] __const__CMD_CONFIG_LOOP_LOWER = 5'd28;
	localparam [4:0] __const__CMD_CONFIG_LOOP_UPPER = 5'd29;
	localparam [4:0] __const__CMD_CONFIG_LOOP_STEP = 5'd30;
	localparam [5:0] __const__CMD_DMA_DONE = 6'd51;
	localparam [3:0] __const__addr_offset_nbits_at_capture_addr_dst_id = 4'd15;
	wire [0:0] addr2controller_lut [0:0];
	reg [0:0] addr_dst_id;
	reg [31:0] dma_bytes;
	reg [31:0] dma_dram_addr_hi;
	reg [31:0] dma_dram_addr_lo;
	reg [31:0] dma_spm_addr;
	reg [7:0] dma_tag;
	wire [0:0] idTo2d_x_lut [0:0];
	wire [0:0] idTo2d_y_lut [0:0];
	wire [0:0] reduce_rdy;
	wire [0:0] crossbar__clk;
	wire [0:0] crossbar__reset;
	reg [1014:0] crossbar__recv__msg;
	wire [4:0] crossbar__recv__rdy;
	reg [4:0] crossbar__recv__val;
	wire [202:0] crossbar__send__msg;
	reg [0:0] crossbar__send__rdy;
	wire [0:0] crossbar__send__val;
	XbarRTL__e29f75d9969a53ec crossbar(
		.clk(crossbar__clk),
		.reset(crossbar__reset),
		.recv__msg(crossbar__recv__msg),
		.recv__rdy(crossbar__recv__rdy),
		.recv__val(crossbar__recv__val),
		.send__msg(crossbar__send__msg),
		.send__rdy(crossbar__send__rdy),
		.send__val(crossbar__send__val)
	);
	wire [0:0] recv_from_cpu_pkt_queue__clk;
	wire [1:0] recv_from_cpu_pkt_queue__count;
	wire [0:0] recv_from_cpu_pkt_queue__reset;
	wire [197:0] recv_from_cpu_pkt_queue__recv__msg;
	wire [0:0] recv_from_cpu_pkt_queue__recv__rdy;
	wire [0:0] recv_from_cpu_pkt_queue__recv__val;
	wire [197:0] recv_from_cpu_pkt_queue__send__msg;
	reg [0:0] recv_from_cpu_pkt_queue__send__rdy;
	wire [0:0] recv_from_cpu_pkt_queue__send__val;
	NormalQueueRTL__3f50370143e38621 recv_from_cpu_pkt_queue(
		.clk(recv_from_cpu_pkt_queue__clk),
		.count(recv_from_cpu_pkt_queue__count),
		.reset(recv_from_cpu_pkt_queue__reset),
		.recv__msg(recv_from_cpu_pkt_queue__recv__msg),
		.recv__rdy(recv_from_cpu_pkt_queue__recv__rdy),
		.recv__val(recv_from_cpu_pkt_queue__recv__val),
		.send__msg(recv_from_cpu_pkt_queue__send__msg),
		.send__rdy(recv_from_cpu_pkt_queue__send__rdy),
		.send__val(recv_from_cpu_pkt_queue__send__val)
	);
	wire [0:0] recv_from_tile_load_request_pkt_queue__clk;
	wire [0:0] recv_from_tile_load_request_pkt_queue__reset;
	wire [201:0] recv_from_tile_load_request_pkt_queue__recv__msg;
	wire [0:0] recv_from_tile_load_request_pkt_queue__recv__rdy;
	wire [0:0] recv_from_tile_load_request_pkt_queue__recv__val;
	wire [201:0] recv_from_tile_load_request_pkt_queue__send__msg;
	reg [0:0] recv_from_tile_load_request_pkt_queue__send__rdy;
	wire [0:0] recv_from_tile_load_request_pkt_queue__send__val;
	ChannelRTL__14ea05451db65d6a recv_from_tile_load_request_pkt_queue(
		.clk(recv_from_tile_load_request_pkt_queue__clk),
		.reset(recv_from_tile_load_request_pkt_queue__reset),
		.recv__msg(recv_from_tile_load_request_pkt_queue__recv__msg),
		.recv__rdy(recv_from_tile_load_request_pkt_queue__recv__rdy),
		.recv__val(recv_from_tile_load_request_pkt_queue__recv__val),
		.send__msg(recv_from_tile_load_request_pkt_queue__send__msg),
		.send__rdy(recv_from_tile_load_request_pkt_queue__send__rdy),
		.send__val(recv_from_tile_load_request_pkt_queue__send__val)
	);
	wire [0:0] recv_from_tile_load_response_pkt_queue__clk;
	wire [0:0] recv_from_tile_load_response_pkt_queue__reset;
	wire [201:0] recv_from_tile_load_response_pkt_queue__recv__msg;
	wire [0:0] recv_from_tile_load_response_pkt_queue__recv__rdy;
	wire [0:0] recv_from_tile_load_response_pkt_queue__recv__val;
	wire [201:0] recv_from_tile_load_response_pkt_queue__send__msg;
	reg [0:0] recv_from_tile_load_response_pkt_queue__send__rdy;
	wire [0:0] recv_from_tile_load_response_pkt_queue__send__val;
	ChannelRTL__14ea05451db65d6a recv_from_tile_load_response_pkt_queue(
		.clk(recv_from_tile_load_response_pkt_queue__clk),
		.reset(recv_from_tile_load_response_pkt_queue__reset),
		.recv__msg(recv_from_tile_load_response_pkt_queue__recv__msg),
		.recv__rdy(recv_from_tile_load_response_pkt_queue__recv__rdy),
		.recv__val(recv_from_tile_load_response_pkt_queue__recv__val),
		.send__msg(recv_from_tile_load_response_pkt_queue__send__msg),
		.send__rdy(recv_from_tile_load_response_pkt_queue__send__rdy),
		.send__val(recv_from_tile_load_response_pkt_queue__send__val)
	);
	wire [0:0] recv_from_tile_store_request_pkt_queue__clk;
	wire [0:0] recv_from_tile_store_request_pkt_queue__reset;
	wire [201:0] recv_from_tile_store_request_pkt_queue__recv__msg;
	wire [0:0] recv_from_tile_store_request_pkt_queue__recv__rdy;
	wire [0:0] recv_from_tile_store_request_pkt_queue__recv__val;
	wire [201:0] recv_from_tile_store_request_pkt_queue__send__msg;
	reg [0:0] recv_from_tile_store_request_pkt_queue__send__rdy;
	wire [0:0] recv_from_tile_store_request_pkt_queue__send__val;
	ChannelRTL__14ea05451db65d6a recv_from_tile_store_request_pkt_queue(
		.clk(recv_from_tile_store_request_pkt_queue__clk),
		.reset(recv_from_tile_store_request_pkt_queue__reset),
		.recv__msg(recv_from_tile_store_request_pkt_queue__recv__msg),
		.recv__rdy(recv_from_tile_store_request_pkt_queue__recv__rdy),
		.recv__val(recv_from_tile_store_request_pkt_queue__recv__val),
		.send__msg(recv_from_tile_store_request_pkt_queue__send__msg),
		.send__rdy(recv_from_tile_store_request_pkt_queue__send__rdy),
		.send__val(recv_from_tile_store_request_pkt_queue__send__val)
	);
	wire [0:0] send_to_cpu_pkt_queue__clk;
	wire [1:0] send_to_cpu_pkt_queue__count;
	wire [0:0] send_to_cpu_pkt_queue__reset;
	reg [197:0] send_to_cpu_pkt_queue__recv__msg;
	wire [0:0] send_to_cpu_pkt_queue__recv__rdy;
	reg [0:0] send_to_cpu_pkt_queue__recv__val;
	wire [197:0] send_to_cpu_pkt_queue__send__msg;
	wire [0:0] send_to_cpu_pkt_queue__send__rdy;
	wire [0:0] send_to_cpu_pkt_queue__send__val;
	NormalQueueRTL__3f50370143e38621 send_to_cpu_pkt_queue(
		.clk(send_to_cpu_pkt_queue__clk),
		.count(send_to_cpu_pkt_queue__count),
		.reset(send_to_cpu_pkt_queue__reset),
		.recv__msg(send_to_cpu_pkt_queue__recv__msg),
		.recv__rdy(send_to_cpu_pkt_queue__recv__rdy),
		.recv__val(send_to_cpu_pkt_queue__recv__val),
		.send__msg(send_to_cpu_pkt_queue__send__msg),
		.send__rdy(send_to_cpu_pkt_queue__send__rdy),
		.send__val(send_to_cpu_pkt_queue__send__val)
	);
	wire [0:0] send_to_mem_load_request_queue__clk;
	wire [0:0] send_to_mem_load_request_queue__reset;
	reg [201:0] send_to_mem_load_request_queue__recv__msg;
	wire [0:0] send_to_mem_load_request_queue__recv__rdy;
	reg [0:0] send_to_mem_load_request_queue__recv__val;
	wire [201:0] send_to_mem_load_request_queue__send__msg;
	wire [0:0] send_to_mem_load_request_queue__send__rdy;
	wire [0:0] send_to_mem_load_request_queue__send__val;
	ChannelRTL__14ea05451db65d6a send_to_mem_load_request_queue(
		.clk(send_to_mem_load_request_queue__clk),
		.reset(send_to_mem_load_request_queue__reset),
		.recv__msg(send_to_mem_load_request_queue__recv__msg),
		.recv__rdy(send_to_mem_load_request_queue__recv__rdy),
		.recv__val(send_to_mem_load_request_queue__recv__val),
		.send__msg(send_to_mem_load_request_queue__send__msg),
		.send__rdy(send_to_mem_load_request_queue__send__rdy),
		.send__val(send_to_mem_load_request_queue__send__val)
	);
	wire [0:0] send_to_mem_store_request_queue__clk;
	wire [0:0] send_to_mem_store_request_queue__reset;
	reg [201:0] send_to_mem_store_request_queue__recv__msg;
	wire [0:0] send_to_mem_store_request_queue__recv__rdy;
	reg [0:0] send_to_mem_store_request_queue__recv__val;
	wire [201:0] send_to_mem_store_request_queue__send__msg;
	wire [0:0] send_to_mem_store_request_queue__send__rdy;
	wire [0:0] send_to_mem_store_request_queue__send__val;
	ChannelRTL__14ea05451db65d6a send_to_mem_store_request_queue(
		.clk(send_to_mem_store_request_queue__clk),
		.reset(send_to_mem_store_request_queue__reset),
		.recv__msg(send_to_mem_store_request_queue__recv__msg),
		.recv__rdy(send_to_mem_store_request_queue__recv__rdy),
		.recv__val(send_to_mem_store_request_queue__recv__val),
		.send__msg(send_to_mem_store_request_queue__send__msg),
		.send__rdy(send_to_mem_store_request_queue__send__rdy),
		.send__val(send_to_mem_store_request_queue__send__val)
	);
	wire [0:0] send_to_tile_load_response_queue__clk;
	wire [0:0] send_to_tile_load_response_queue__reset;
	reg [201:0] send_to_tile_load_response_queue__recv__msg;
	wire [0:0] send_to_tile_load_response_queue__recv__rdy;
	reg [0:0] send_to_tile_load_response_queue__recv__val;
	wire [201:0] send_to_tile_load_response_queue__send__msg;
	wire [0:0] send_to_tile_load_response_queue__send__rdy;
	wire [0:0] send_to_tile_load_response_queue__send__val;
	ChannelRTL__14ea05451db65d6a send_to_tile_load_response_queue(
		.clk(send_to_tile_load_response_queue__clk),
		.reset(send_to_tile_load_response_queue__reset),
		.recv__msg(send_to_tile_load_response_queue__recv__msg),
		.recv__rdy(send_to_tile_load_response_queue__recv__rdy),
		.recv__val(send_to_tile_load_response_queue__recv__val),
		.send__msg(send_to_tile_load_response_queue__send__msg),
		.send__rdy(send_to_tile_load_response_queue__send__rdy),
		.send__val(send_to_tile_load_response_queue__send__val)
	);
	reg [172:0] __tmpvar__update_dma_cmd_regs_cpu_payload;
	reg [5:0] __tmpvar__update_dma_cmd_regs_cpu_cmd;
	reg [31:0] __tmpvar__update_dma_cmd_regs_cpu_data;
	reg [172:0] __tmpvar__update_received_msg_cpu_payload;
	reg [5:0] __tmpvar__update_received_msg_cpu_cmd;
	reg [201:0] __tmpvar__update_received_msg_received_pkt;
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : capture_addr_dst_id
		if (_sv2v_0)
			;
		addr_dst_id = addr2controller_lut[sv2v_cast_1(crossbar__send__msg[(1'd0 * 203) + 131-:15] >> __const__addr_offset_nbits_at_capture_addr_dst_id)];
	end
	always @(*) begin : update_dma_spm_forwarding
		if (_sv2v_0)
			;
		if (__const__has_dma_ports_at_update_dma_spm_forwarding) begin
			send_to_sram_store_request_from_dma__val = recv_from_dma_spm_wr_req__val;
			recv_from_dma_spm_wr_req__rdy = send_to_sram_store_request_from_dma__rdy;
			send_to_sram_store_request_from_dma__msg = recv_from_dma_spm_wr_req__msg;
			send_to_sram_load_request_from_dma__val = recv_from_dma_spm_rd_req__val;
			recv_from_dma_spm_rd_req__rdy = send_to_sram_load_request_from_dma__rdy;
			send_to_sram_load_request_from_dma__msg = recv_from_dma_spm_rd_req__msg;
			send_to_dma_spm_rd_resp__val = recv_from_sram_load_response__val;
			recv_from_sram_load_response__rdy = send_to_dma_spm_rd_resp__rdy;
			send_to_dma_spm_rd_resp__msg = recv_from_sram_load_response__msg;
		end
		else begin
			send_to_sram_store_request_from_dma__val = 1'd0;
			send_to_sram_store_request_from_dma__msg = 68'h00000000000000000;
			send_to_sram_load_request_from_dma__val = 1'd0;
			send_to_sram_load_request_from_dma__msg = 32'd0;
			recv_from_sram_load_response__rdy = 1'd0;
			recv_from_dma_spm_wr_req__rdy = 1'd0;
			recv_from_dma_spm_rd_req__rdy = 1'd0;
			send_to_dma_spm_rd_resp__val = 1'd0;
			send_to_dma_spm_rd_resp__msg = 32'd0;
		end
	end
	function automatic [2:0] sv2v_cast_3;
		input reg [2:0] inp;
		sv2v_cast_3 = inp;
	endfunction
	function automatic [5:0] sv2v_cast_6;
		input reg [5:0] inp;
		sv2v_cast_6 = inp;
	endfunction
	always @(*) begin : update_received_msg
		if (_sv2v_0)
			;
		send_to_cpu_pkt_queue__recv__val = 1'd0;
		send_to_cpu_pkt_queue__recv__msg = 198'h00000000000000000000000000000000000000000000000000;
		recv_from_ctrl_ring_pkt__rdy = 1'd0;
		dma_cmd__val = 1'd0;
		dma_cmd__msg = {3'd0, dma_dram_addr_hi, dma_dram_addr_lo, dma_spm_addr, dma_bytes, dma_tag};
		dma_done__rdy = 1'd0;
		send_to_im2col_engine_pkt__val = 1'd0;
		send_to_im2col_engine_pkt__msg = 198'h00000000000000000000000000000000000000000000000000;
		crossbar__recv__val[4 - sv2v_cast_3(__const__kFromCpuCtrlAndDataIdx)+:1] = 1'd0;
		crossbar__recv__msg[(4 - sv2v_cast_3(__const__kFromCpuCtrlAndDataIdx)) * 203+:203] = 203'h000000000000000000000000000000000000000000000000000;
		crossbar__recv__val[4 - __const__kFromInterTileRingIdx+:1] = recv_from_ctrl_ring_pkt__val;
		recv_from_ctrl_ring_pkt__rdy = crossbar__recv__rdy[4 - __const__kFromInterTileRingIdx+:1];
		crossbar__recv__msg[(4 - __const__kFromInterTileRingIdx) * 203+:203] = {1'd0, cgra_id, recv_from_ctrl_ring_pkt__msg[186], idTo2d_x_lut[cgra_id], idTo2d_y_lut[cgra_id], recv_from_ctrl_ring_pkt__msg[183], recv_from_ctrl_ring_pkt__msg[182], recv_from_ctrl_ring_pkt__msg[197-:5], recv_from_ctrl_ring_pkt__msg[192-:5], 13'h0000, recv_from_ctrl_ring_pkt__msg[172-:173]};
		crossbar__recv__val[4 - sv2v_cast_3(__const__kLoadRequestInportIdx)+:1] = recv_from_tile_load_request_pkt_queue__send__val;
		recv_from_tile_load_request_pkt_queue__send__rdy = crossbar__recv__rdy[4 - sv2v_cast_3(__const__kLoadRequestInportIdx)+:1];
		crossbar__recv__msg[(4 - sv2v_cast_3(__const__kLoadRequestInportIdx)) * 203+:203] = {1'd0, recv_from_tile_load_request_pkt_queue__send__msg};
		crossbar__recv__val[4 - sv2v_cast_3(__const__kStoreRequestInportIdx)+:1] = recv_from_tile_store_request_pkt_queue__send__val;
		recv_from_tile_store_request_pkt_queue__send__rdy = crossbar__recv__rdy[4 - sv2v_cast_3(__const__kStoreRequestInportIdx)+:1];
		crossbar__recv__msg[(4 - sv2v_cast_3(__const__kStoreRequestInportIdx)) * 203+:203] = {1'd0, recv_from_tile_store_request_pkt_queue__send__msg};
		crossbar__recv__val[4 - sv2v_cast_3(__const__kLoadResponseInportIdx)+:1] = recv_from_tile_load_response_pkt_queue__send__val;
		recv_from_tile_load_response_pkt_queue__send__rdy = crossbar__recv__rdy[4 - sv2v_cast_3(__const__kLoadResponseInportIdx)+:1];
		crossbar__recv__msg[(4 - sv2v_cast_3(__const__kLoadResponseInportIdx)) * 203+:203] = {1'd0, recv_from_tile_load_response_pkt_queue__send__msg};
		__tmpvar__update_received_msg_cpu_payload = recv_from_cpu_pkt_queue__send__msg[172-:173];
		__tmpvar__update_received_msg_cpu_cmd = __tmpvar__update_received_msg_cpu_payload[172-:6];
		if (__const__has_dma_ports_at_update_received_msg & (((((__tmpvar__update_received_msg_cpu_cmd == __const__CMD_DMA_CONFIG_DRAM_ADDR_LO) | (__tmpvar__update_received_msg_cpu_cmd == __const__CMD_DMA_CONFIG_DRAM_ADDR_HI)) | (__tmpvar__update_received_msg_cpu_cmd == __const__CMD_DMA_CONFIG_SPM_ADDR)) | (__tmpvar__update_received_msg_cpu_cmd == __const__CMD_DMA_CONFIG_BYTES)) | (__tmpvar__update_received_msg_cpu_cmd == __const__CMD_DMA_CONFIG_TAG)))
			recv_from_cpu_pkt_queue__send__rdy = 1'd1;
		else if (__const__has_dma_ports_at_update_received_msg & ((__tmpvar__update_received_msg_cpu_cmd == __const__CMD_DMA_MVIN) | (__tmpvar__update_received_msg_cpu_cmd == __const__CMD_DMA_MVOUT))) begin
			dma_cmd__val = recv_from_cpu_pkt_queue__send__val;
			if (__tmpvar__update_received_msg_cpu_cmd == __const__CMD_DMA_MVIN)
				dma_cmd__msg = {3'd0, dma_dram_addr_hi, dma_dram_addr_lo, dma_spm_addr, dma_bytes, dma_tag};
			else
				dma_cmd__msg = {3'd1, dma_dram_addr_hi, dma_dram_addr_lo, dma_spm_addr, dma_bytes, dma_tag};
			recv_from_cpu_pkt_queue__send__rdy = dma_cmd__rdy;
		end
		else if (__const__has_im2col_engine_at_update_received_msg & (((((((__tmpvar__update_received_msg_cpu_cmd == __const__CMD_IM2COL_LAUNCH) | (__tmpvar__update_received_msg_cpu_cmd == __const__CMD_IM2COL_H)) | (__tmpvar__update_received_msg_cpu_cmd == __const__CMD_IM2COL_W)) | (__tmpvar__update_received_msg_cpu_cmd == __const__CMD_IM2COL_KH)) | (__tmpvar__update_received_msg_cpu_cmd == __const__CMD_IM2COL_KW)) | (__tmpvar__update_received_msg_cpu_cmd == __const__CMD_IM2COL_LOG2_STRIDE)) | (__tmpvar__update_received_msg_cpu_cmd == __const__CMD_IM2COL_DST_SRAM_BASE))) begin
			send_to_im2col_engine_pkt__val = recv_from_cpu_pkt_queue__send__val;
			send_to_im2col_engine_pkt__msg = recv_from_cpu_pkt_queue__send__msg;
			recv_from_cpu_pkt_queue__send__rdy = send_to_im2col_engine_pkt__rdy;
		end
		else begin
			crossbar__recv__val[4 - sv2v_cast_3(__const__kFromCpuCtrlAndDataIdx)+:1] = recv_from_cpu_pkt_queue__send__val;
			recv_from_cpu_pkt_queue__send__rdy = crossbar__recv__rdy[4 - sv2v_cast_3(__const__kFromCpuCtrlAndDataIdx)+:1];
			crossbar__recv__msg[(4 - sv2v_cast_3(__const__kFromCpuCtrlAndDataIdx)) * 203+:203] = {1'd0, cgra_id, recv_from_cpu_pkt_queue__send__msg[186], 2'h0, idTo2d_x_lut[recv_from_cpu_pkt_queue__send__msg[186]], idTo2d_y_lut[recv_from_cpu_pkt_queue__send__msg[186]], __const__num_tiles_at_update_received_msg, recv_from_cpu_pkt_queue__send__msg[192-:5], 13'h0000, recv_from_cpu_pkt_queue__send__msg[172-:173]};
		end
		send_to_mem_load_request_queue__recv__val = 1'd0;
		send_to_mem_store_request_queue__recv__val = 1'd0;
		send_to_tile_load_response_queue__recv__val = 1'd0;
		send_to_mem_load_request_queue__recv__msg = 202'h000000000000000000000000000000000000000000000000000;
		send_to_mem_store_request_queue__recv__msg = 202'h000000000000000000000000000000000000000000000000000;
		send_to_tile_load_response_queue__recv__msg = 202'h000000000000000000000000000000000000000000000000000;
		recv_from_inter_cgra_noc__rdy = 1'd0;
		send_to_ctrl_ring_pkt__val = 1'd0;
		send_to_ctrl_ring_pkt__msg = 198'h00000000000000000000000000000000000000000000000000;
		__tmpvar__update_received_msg_received_pkt = recv_from_inter_cgra_noc__msg;
		if (recv_from_inter_cgra_noc__val) begin
			if (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_LOAD_REQUEST)) begin
				send_to_mem_load_request_queue__recv__val = 1'd1;
				if (send_to_mem_load_request_queue__recv__rdy) begin
					recv_from_inter_cgra_noc__rdy = 1'd1;
					send_to_mem_load_request_queue__recv__msg = __tmpvar__update_received_msg_received_pkt;
				end
			end
			else if (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_STORE_REQUEST)) begin
				send_to_mem_store_request_queue__recv__msg = __tmpvar__update_received_msg_received_pkt;
				send_to_mem_store_request_queue__recv__val = 1'd1;
				if (send_to_mem_store_request_queue__recv__rdy)
					recv_from_inter_cgra_noc__rdy = 1'd1;
			end
			else if (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_LOAD_RESPONSE)) begin
				if (recv_from_inter_cgra_noc__msg[190-:5] == __const__num_tiles_at_update_received_msg) begin
					recv_from_inter_cgra_noc__rdy = send_to_cpu_pkt_queue__recv__rdy;
					send_to_cpu_pkt_queue__recv__val = 1'd1;
					send_to_cpu_pkt_queue__recv__msg = {recv_from_inter_cgra_noc__msg[195-:5], recv_from_inter_cgra_noc__msg[190-:5], recv_from_inter_cgra_noc__msg[201], recv_from_inter_cgra_noc__msg[200], recv_from_inter_cgra_noc__msg[199], recv_from_inter_cgra_noc__msg[198], recv_from_inter_cgra_noc__msg[197], recv_from_inter_cgra_noc__msg[196], 9'h000, recv_from_inter_cgra_noc__msg[172-:173]};
				end
				else begin
					recv_from_inter_cgra_noc__rdy = send_to_tile_load_response_queue__recv__rdy;
					send_to_tile_load_response_queue__recv__msg = __tmpvar__update_received_msg_received_pkt;
					send_to_tile_load_response_queue__recv__val = 1'd1;
				end
			end
			else if (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_COMPLETE)) begin
				recv_from_inter_cgra_noc__rdy = send_to_cpu_pkt_queue__recv__rdy;
				send_to_cpu_pkt_queue__recv__val = 1'd1;
				send_to_cpu_pkt_queue__recv__msg = {recv_from_inter_cgra_noc__msg[195-:5], recv_from_inter_cgra_noc__msg[190-:5], recv_from_inter_cgra_noc__msg[201], recv_from_inter_cgra_noc__msg[200], recv_from_inter_cgra_noc__msg[199], recv_from_inter_cgra_noc__msg[198], recv_from_inter_cgra_noc__msg[197], recv_from_inter_cgra_noc__msg[196], 9'h000, recv_from_inter_cgra_noc__msg[172-:173]};
			end
			else if (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_LEAF_COUNTER_COMPLETE))
				recv_from_inter_cgra_noc__rdy = 1'd1;
			else if (__const__has_global_reduce_at_update_received_msg & ((recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_GLOBAL_REDUCE_ADD)) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_GLOBAL_REDUCE_COUNT))))
				recv_from_inter_cgra_noc__rdy = reduce_rdy;
			else if (((((((((((((((((((((((recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG)) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_PROLOGUE_FU))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_PROLOGUE_FU_CROSSBAR))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_TOTAL_CTRL_COUNT))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_COUNT_PER_ITER))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_CTRL_LOWER_BOUND))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_CONST))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_GLOBAL_REDUCE_ADD_RESPONSE))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_GLOBAL_REDUCE_MUL_RESPONSE))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_STREAMING_LD_START_ADDR))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_STREAMING_LD_STRIDE))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_STREAMING_LD_END_ADDR))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_PAUSE))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_PRESERVE))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_RESUME))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_RECORD_PHI_ADDR))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_TERMINATE))) | (recv_from_inter_cgra_noc__msg[172-:6] == __const__CMD_REARM)) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_LAUNCH))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_LOOP_LOWER))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_LOOP_UPPER))) | (recv_from_inter_cgra_noc__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_LOOP_STEP))) begin
				recv_from_inter_cgra_noc__rdy = send_to_ctrl_ring_pkt__rdy;
				send_to_ctrl_ring_pkt__val = recv_from_inter_cgra_noc__val;
				send_to_ctrl_ring_pkt__msg = {recv_from_inter_cgra_noc__msg[195-:5], recv_from_inter_cgra_noc__msg[190-:5], recv_from_inter_cgra_noc__msg[201], recv_from_inter_cgra_noc__msg[200], recv_from_inter_cgra_noc__msg[199], recv_from_inter_cgra_noc__msg[198], recv_from_inter_cgra_noc__msg[197], recv_from_inter_cgra_noc__msg[196], 9'h000, recv_from_inter_cgra_noc__msg[172-:173]};
			end
		end
		if (__const__has_dma_ports_at_update_received_msg & dma_done__val) begin
			if ((__tmpvar__update_received_msg_received_pkt[172-:6] == sv2v_cast_6(__const__CMD_COMPLETE)) | ((__tmpvar__update_received_msg_received_pkt[172-:6] == sv2v_cast_6(__const__CMD_LOAD_RESPONSE)) & (__tmpvar__update_received_msg_received_pkt[190-:5] == __const__num_tiles_at_update_received_msg)))
				recv_from_inter_cgra_noc__rdy = 1'd0;
			dma_done__rdy = send_to_cpu_pkt_queue__recv__rdy;
			send_to_cpu_pkt_queue__recv__val = 1'd1;
			send_to_cpu_pkt_queue__recv__msg = {__const__num_tiles_at_update_received_msg, __const__num_tiles_at_update_received_msg, cgra_id, cgra_id, idTo2d_x_lut[cgra_id], idTo2d_y_lut[cgra_id], idTo2d_x_lut[cgra_id], idTo2d_y_lut[cgra_id], dma_done__msg[7-:8], 1'd0, __const__CMD_DMA_DONE, {24 {1'b0}}, dma_done__msg[7-:8], 135'h4000000000000000000000000000000000};
		end
	end
	always @(*) begin : update_sending_to_noc_msg
		if (_sv2v_0)
			;
		send_to_inter_cgra_noc__val = crossbar__send__val[1'd0+:1];
		crossbar__send__rdy[1'd0+:1] = send_to_inter_cgra_noc__rdy;
		send_to_inter_cgra_noc__msg = crossbar__send__msg[(1'd0 * 203) + 201-:202];
		if ((crossbar__send__msg[(1'd0 * 203) + 172-:6] == sv2v_cast_6(__const__CMD_LOAD_REQUEST)) | (crossbar__send__msg[(1'd0 * 203) + 172-:6] == sv2v_cast_6(__const__CMD_STORE_REQUEST))) begin
			send_to_inter_cgra_noc__msg[200] = addr_dst_id;
			send_to_inter_cgra_noc__msg[197] = idTo2d_x_lut[addr_dst_id];
			send_to_inter_cgra_noc__msg[196] = idTo2d_y_lut[addr_dst_id];
		end
	end
	function automatic [7:0] sv2v_cast_8;
		input reg [7:0] inp;
		sv2v_cast_8 = inp;
	endfunction
	always @(posedge clk) begin : update_dma_cmd_regs
		if (reset) begin
			dma_dram_addr_lo <= 32'd0;
			dma_dram_addr_hi <= 32'd0;
			dma_spm_addr <= 32'd0;
			dma_bytes <= 32'd0;
			dma_tag <= 8'd0;
		end
		else if (__const__has_dma_ports_at_update_dma_cmd_regs) begin
			__tmpvar__update_dma_cmd_regs_cpu_payload = recv_from_cpu_pkt_queue__send__msg[172-:173];
			__tmpvar__update_dma_cmd_regs_cpu_cmd = __tmpvar__update_dma_cmd_regs_cpu_payload[172-:6];
			__tmpvar__update_dma_cmd_regs_cpu_data = __tmpvar__update_dma_cmd_regs_cpu_payload[166-:32];
			if (recv_from_cpu_pkt_queue__send__val & recv_from_cpu_pkt_queue__send__rdy) begin
				if (__tmpvar__update_dma_cmd_regs_cpu_cmd == __const__CMD_DMA_CONFIG_DRAM_ADDR_LO)
					dma_dram_addr_lo <= __tmpvar__update_dma_cmd_regs_cpu_data;
				else if (__tmpvar__update_dma_cmd_regs_cpu_cmd == __const__CMD_DMA_CONFIG_DRAM_ADDR_HI)
					dma_dram_addr_hi <= __tmpvar__update_dma_cmd_regs_cpu_data;
				else if (__tmpvar__update_dma_cmd_regs_cpu_cmd == __const__CMD_DMA_CONFIG_SPM_ADDR)
					dma_spm_addr <= {{17 {1'b0}}, __tmpvar__update_dma_cmd_regs_cpu_payload[131-:15]};
				else if (__tmpvar__update_dma_cmd_regs_cpu_cmd == __const__CMD_DMA_CONFIG_BYTES)
					dma_bytes <= __tmpvar__update_dma_cmd_regs_cpu_data;
				else if (__tmpvar__update_dma_cmd_regs_cpu_cmd == __const__CMD_DMA_CONFIG_TAG)
					dma_tag <= sv2v_cast_8(__tmpvar__update_dma_cmd_regs_cpu_data);
			end
		end
	end
	assign recv_from_tile_load_request_pkt_queue__clk = clk;
	assign recv_from_tile_load_request_pkt_queue__reset = reset;
	assign recv_from_tile_load_response_pkt_queue__clk = clk;
	assign recv_from_tile_load_response_pkt_queue__reset = reset;
	assign recv_from_tile_store_request_pkt_queue__clk = clk;
	assign recv_from_tile_store_request_pkt_queue__reset = reset;
	assign send_to_mem_load_request_queue__clk = clk;
	assign send_to_mem_load_request_queue__reset = reset;
	assign send_to_tile_load_response_queue__clk = clk;
	assign send_to_tile_load_response_queue__reset = reset;
	assign send_to_mem_store_request_queue__clk = clk;
	assign send_to_mem_store_request_queue__reset = reset;
	assign crossbar__clk = clk;
	assign crossbar__reset = reset;
	assign recv_from_cpu_pkt_queue__clk = clk;
	assign recv_from_cpu_pkt_queue__reset = reset;
	assign send_to_cpu_pkt_queue__clk = clk;
	assign send_to_cpu_pkt_queue__reset = reset;
	assign addr2controller_lut[0] = 1'd0;
	assign idTo2d_x_lut[0] = 1'd0;
	assign idTo2d_y_lut[0] = 1'd0;
	assign recv_from_tile_load_request_pkt_queue__recv__msg = recv_from_tile_load_request_pkt__msg;
	assign recv_from_tile_load_request_pkt__rdy = recv_from_tile_load_request_pkt_queue__recv__rdy;
	assign recv_from_tile_load_request_pkt_queue__recv__val = recv_from_tile_load_request_pkt__val;
	assign recv_from_tile_load_response_pkt_queue__recv__msg = recv_from_tile_load_response_pkt__msg;
	assign recv_from_tile_load_response_pkt__rdy = recv_from_tile_load_response_pkt_queue__recv__rdy;
	assign recv_from_tile_load_response_pkt_queue__recv__val = recv_from_tile_load_response_pkt__val;
	assign recv_from_tile_store_request_pkt_queue__recv__msg = recv_from_tile_store_request_pkt__msg;
	assign recv_from_tile_store_request_pkt__rdy = recv_from_tile_store_request_pkt_queue__recv__rdy;
	assign recv_from_tile_store_request_pkt_queue__recv__val = recv_from_tile_store_request_pkt__val;
	assign send_to_sram_load_request_from_noc__msg = send_to_mem_load_request_queue__send__msg;
	assign send_to_mem_load_request_queue__send__rdy = send_to_sram_load_request_from_noc__rdy;
	assign send_to_sram_load_request_from_noc__val = send_to_mem_load_request_queue__send__val;
	assign send_to_tile_load_response__msg = send_to_tile_load_response_queue__send__msg;
	assign send_to_tile_load_response_queue__send__rdy = send_to_tile_load_response__rdy;
	assign send_to_tile_load_response__val = send_to_tile_load_response_queue__send__val;
	assign send_to_sram_store_request_from_noc__msg = send_to_mem_store_request_queue__send__msg;
	assign send_to_mem_store_request_queue__send__rdy = send_to_sram_store_request_from_noc__rdy;
	assign send_to_sram_store_request_from_noc__val = send_to_mem_store_request_queue__send__val;
	assign recv_from_cpu_pkt_queue__recv__msg = recv_from_cpu_pkt__msg;
	assign recv_from_cpu_pkt__rdy = recv_from_cpu_pkt_queue__recv__rdy;
	assign recv_from_cpu_pkt_queue__recv__val = recv_from_cpu_pkt__val;
	assign send_to_cpu_pkt__msg = send_to_cpu_pkt_queue__send__msg;
	assign send_to_cpu_pkt_queue__send__rdy = send_to_cpu_pkt__rdy;
	assign send_to_cpu_pkt__val = send_to_cpu_pkt_queue__send__val;
	assign recv_from_im2col_pkt__rdy = 1'd0;
	assign reduce_rdy = 1'd0;
	initial _sv2v_0 = 0;
endmodule
module RingNetworkRTL__ad9b2c0aa02e2f3a (
	clk,
	reset,
	recv__msg,
	recv__rdy,
	recv__val,
	send__msg,
	send__rdy,
	send__val
);
	reg _sv2v_0;
	input wire [0:0] clk;
	input wire [0:0] reset;
	input wire [3365:0] recv__msg;
	output wire [16:0] recv__rdy;
	input wire [16:0] recv__val;
	output wire [3365:0] send__msg;
	input wire [16:0] send__rdy;
	output wire [16:0] send__val;
	reg [16:0] full;
	wire [16:0] eligible;
	reg [16:0] priority_mask;
	wire [16:0] masked;
	wire [16:0] choices;
	wire [16:0] grant;
	wire [16:0] next_mask;
	reg [197:0] words [0:16];
	reg [197:0] packet;
	wire accepted;
	genvar _gv_i_1;
	generate
		for (_gv_i_1 = 0; _gv_i_1 < 17; _gv_i_1 = _gv_i_1 + 1) begin : ports
			localparam i = _gv_i_1;
			assign eligible[i] = ((!reset && recv__val[16 - i+:1]) && (recv__msg[((16 - i) * 198) + 192-:5] < 17)) && !full[recv__msg[((16 - i) * 198) + 192-:5]];
			assign recv__rdy[16 - i+:1] = grant[i];
			assign send__val[16 - i+:1] = full[i];
			assign send__msg[(16 - i) * 198+:198] = words[i];
			if (i == 0) begin : first
				assign next_mask[i] = 1'b0;
			end
			else begin : following
				assign next_mask[i] = |grant[i - 1:0];
			end
		end
	endgenerate
	assign masked = eligible & priority_mask;
	assign choices = (|masked ? masked : eligible);
	assign grant = choices & (~choices + 17'd1);
	assign accepted = |grant;
	always @(*) begin
		if (_sv2v_0)
			;
		packet = 0;
		begin : sv2v_autoblock_1
			integer i;
			for (i = 0; i < 17; i = i + 1)
				packet = packet | (recv__msg[(16 - i) * 198+:198] & {198 {grant[i]}});
		end
	end
	function automatic signed [4:0] sv2v_cast_5_signed;
		input reg signed [4:0] inp;
		sv2v_cast_5_signed = inp;
	endfunction
	always @(posedge clk)
		if (reset) begin
			full <= 0;
			priority_mask <= 17'h1ffff;
		end
		else begin
			if (accepted)
				priority_mask <= next_mask;
			begin : sv2v_autoblock_2
				integer i;
				for (i = 0; i < 17; i = i + 1)
					begin
						if (full[i] && send__rdy[16 - i+:1])
							full[i] <= 0;
						if (accepted && (packet[192:188] == sv2v_cast_5_signed(i))) begin
							full[i] <= 1;
							words[i] <= packet;
						end
					end
			end
		end
	initial _sv2v_0 = 0;
endmodule
module RegisterFile__5e8ec2fcb9b12d47 (
	clk,
	raddr,
	rdata,
	reset,
	waddr,
	wdata,
	wen
);
	reg _sv2v_0;
	input wire [0:0] clk;
	input wire [5:0] raddr;
	output reg [34:0] rdata;
	input wire [0:0] reset;
	input wire [5:0] waddr;
	input wire [34:0] wdata;
	input wire [0:0] wen;
	localparam [0:0] __const__rd_ports_at_up_rf_read = 1'd1;
	localparam [0:0] __const__wr_ports_at_up_rf_write = 1'd1;
	reg [34:0] regs [0:57];
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : up_rf_read
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__rd_ports_at_up_rf_read; i = i + 1'd1)
				rdata[sv2v_cast_1(i) * 35+:35] = regs[raddr[sv2v_cast_1(i) * 6+:6]];
		end
	end
	always @(posedge clk) begin : up_rf_write
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__wr_ports_at_up_rf_write; i = i + 1'd1)
				if (wen[sv2v_cast_1(i)+:1])
					regs[waddr[sv2v_cast_1(i) * 6+:6]] <= wdata[sv2v_cast_1(i) * 35+:35];
		end
	end
	initial _sv2v_0 = 0;
endmodule
module ConstQueueDynamicRTL__9e94e78d19eca926 (
	clear,
	clk,
	ctrl_proceed,
	reset,
	recv_const__msg,
	recv_const__rdy,
	recv_const__val,
	send_const__msg,
	send_const__rdy,
	send_const__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [0:0] ctrl_proceed;
	input wire [0:0] reset;
	input wire [34:0] recv_const__msg;
	output reg [0:0] recv_const__rdy;
	input wire [0:0] recv_const__val;
	output wire [34:0] send_const__msg;
	input wire [0:0] send_const__rdy;
	output reg [0:0] send_const__val;
	localparam [5:0] __const__const_mem_size_at_load_const = 6'd58;
	localparam [5:0] __const__const_mem_size_at_update_wr_cur = 6'd58;
	reg [0:0] consume_pending;
	reg [5:0] rd_cur;
	reg [5:0] wr_cur;
	wire [0:0] reg_file__clk;
	wire [5:0] reg_file__raddr;
	wire [34:0] reg_file__rdata;
	wire [0:0] reg_file__reset;
	reg [5:0] reg_file__waddr;
	reg [34:0] reg_file__wdata;
	reg [0:0] reg_file__wen;
	RegisterFile__5e8ec2fcb9b12d47 reg_file(
		.clk(reg_file__clk),
		.raddr(reg_file__raddr),
		.rdata(reg_file__rdata),
		.reset(reg_file__reset),
		.waddr(reg_file__waddr),
		.wdata(reg_file__wdata),
		.wen(reg_file__wen)
	);
	reg [0:0] __tmpvar__load_const_not_full;
	reg [0:0] __tmpvar__update_wr_cur_not_full;
	reg [0:0] __tmpvar__update_rd_cur_handshake_now;
	reg [0:0] __tmpvar__update_rd_cur_consume_retire;
	always @(*) begin : load_const
		if (_sv2v_0)
			;
		reg_file__waddr[1'd0 * 6+:6] = 6'd0;
		reg_file__wdata[1'd0 * 35+:35] = 35'h000000000;
		reg_file__wen[1'd0+:1] = 1'd0;
		__tmpvar__load_const_not_full = wr_cur < __const__const_mem_size_at_load_const;
		recv_const__rdy = __tmpvar__load_const_not_full;
		if (recv_const__val & __tmpvar__load_const_not_full) begin
			reg_file__waddr[1'd0 * 6+:6] = wr_cur;
			reg_file__wdata[1'd0 * 35+:35] = recv_const__msg;
			reg_file__wen[1'd0+:1] = 1'd1;
		end
	end
	always @(*) begin : update_send_val
		if (_sv2v_0)
			;
		if (rd_cur < wr_cur)
			send_const__val = 1'd1;
		else
			send_const__val = 1'd0;
	end
	always @(posedge clk) begin : update_rd_cur
		if (reset | clear) begin
			rd_cur <= 6'd0;
			consume_pending <= 1'd0;
		end
		else begin
			__tmpvar__update_rd_cur_handshake_now = send_const__rdy;
			__tmpvar__update_rd_cur_consume_retire = (consume_pending | __tmpvar__update_rd_cur_handshake_now) & ctrl_proceed;
			if (__tmpvar__update_rd_cur_consume_retire) begin
				if (rd_cur < (wr_cur - 6'd1))
					rd_cur <= rd_cur + 6'd1;
				else
					rd_cur <= 6'd0;
				consume_pending <= 1'd0;
			end
			else
				consume_pending <= consume_pending | __tmpvar__update_rd_cur_handshake_now;
		end
	end
	always @(posedge clk) begin : update_wr_cur
		__tmpvar__update_wr_cur_not_full = wr_cur < __const__const_mem_size_at_update_wr_cur;
		if (reset | clear)
			wr_cur <= 6'd0;
		else if (recv_const__val & __tmpvar__update_wr_cur_not_full)
			wr_cur <= wr_cur + 6'd1;
	end
	assign reg_file__clk = clk;
	assign reg_file__reset = reset;
	assign send_const__msg = reg_file__rdata[0+:35];
	assign reg_file__raddr[0+:6] = rd_cur;
	initial _sv2v_0 = 0;
endmodule
module NormalQueueCtrlWithClearRTL__num_entries_2 (
	clear,
	clk,
	count,
	raddr,
	recv_rdy,
	recv_val,
	reset,
	send_rdy,
	send_val,
	waddr,
	wen
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	output reg [1:0] count;
	output wire [0:0] raddr;
	output reg [0:0] recv_rdy;
	input wire [0:0] recv_val;
	input wire [0:0] reset;
	input wire [0:0] send_rdy;
	output reg [0:0] send_val;
	output wire [0:0] waddr;
	output wire [0:0] wen;
	localparam [1:0] __const__num_entries_at__lambda__s_cgra_tile_0__ctrl_mem_recv_from_element_queue_ctrl_recv_rdy = 2'd2;
	localparam [1:0] __const__num_entries_at_up_reg = 2'd2;
	reg [0:0] head;
	reg [0:0] recv_xfer;
	reg [0:0] send_xfer;
	reg [0:0] tail;
	always @(*) begin : _lambda__s_cgra_tile_0__ctrl_mem_recv_from_element_queue_ctrl_recv_rdy
		if (_sv2v_0)
			;
		recv_rdy = count < __const__num_entries_at__lambda__s_cgra_tile_0__ctrl_mem_recv_from_element_queue_ctrl_recv_rdy;
	end
	always @(*) begin : _lambda__s_cgra_tile_0__ctrl_mem_recv_from_element_queue_ctrl_recv_xfer
		if (_sv2v_0)
			;
		recv_xfer = recv_val & recv_rdy;
	end
	always @(*) begin : _lambda__s_cgra_tile_0__ctrl_mem_recv_from_element_queue_ctrl_send_val
		if (_sv2v_0)
			;
		send_val = count > 2'd0;
	end
	always @(*) begin : _lambda__s_cgra_tile_0__ctrl_mem_recv_from_element_queue_ctrl_send_xfer
		if (_sv2v_0)
			;
		send_xfer = send_val & send_rdy;
	end
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(posedge clk) begin : up_reg
		if (reset | clear) begin
			head <= 1'd0;
			tail <= 1'd0;
			count <= 2'd0;
		end
		else begin
			if (recv_xfer)
				tail <= (tail < (sv2v_cast_1(__const__num_entries_at_up_reg) - 1'd1) ? tail + 1'd1 : 1'd0);
			if (send_xfer)
				head <= (head < (sv2v_cast_1(__const__num_entries_at_up_reg) - 1'd1) ? head + 1'd1 : 1'd0);
			if (recv_xfer & ~send_xfer)
				count <= count + 2'd1;
			else if (~recv_xfer & send_xfer)
				count <= count - 2'd1;
		end
	end
	assign wen = recv_xfer;
	assign waddr = tail;
	assign raddr = head;
	initial _sv2v_0 = 0;
endmodule
module CtrlMemDynamicRTL__df08502b9dfbead5 (
	cgra_id,
	clk,
	ctrl_addr_outport,
	prologue_count_outport_fu,
	prologue_count_outport_fu_crossbar,
	prologue_count_outport_routing_crossbar,
	rearm,
	reset,
	tile_id,
	recv_from_element__msg,
	recv_from_element__rdy,
	recv_from_element__val,
	recv_pkt_from_controller__msg,
	recv_pkt_from_controller__rdy,
	recv_pkt_from_controller__val,
	send_ctrl__msg,
	send_ctrl__rdy,
	send_ctrl__val,
	send_pkt_to_controller__msg,
	send_pkt_to_controller__rdy,
	send_pkt_to_controller__val,
	send_to_element__msg,
	send_to_element__rdy,
	send_to_element__val
);
	reg _sv2v_0;
	input wire [0:0] cgra_id;
	input wire [0:0] clk;
	output reg [5:0] ctrl_addr_outport;
	output reg [2:0] prologue_count_outport_fu;
	output reg [287:0] prologue_count_outport_fu_crossbar;
	output reg [1151:0] prologue_count_outport_routing_crossbar;
	output reg [0:0] rearm;
	input wire [0:0] reset;
	input wire [4:0] tile_id;
	input wire [172:0] recv_from_element__msg;
	output wire [0:0] recv_from_element__rdy;
	input wire [0:0] recv_from_element__val;
	input wire [197:0] recv_pkt_from_controller__msg;
	output wire [0:0] recv_pkt_from_controller__rdy;
	input wire [0:0] recv_pkt_from_controller__val;
	output reg [110:0] send_ctrl__msg;
	input wire [0:0] send_ctrl__rdy;
	output reg [0:0] send_ctrl__val;
	output reg [197:0] send_pkt_to_controller__msg;
	input wire [0:0] send_pkt_to_controller__rdy;
	output reg [0:0] send_pkt_to_controller__val;
	output reg [172:0] send_to_element__msg;
	input wire [0:0] send_to_element__rdy;
	output reg [0:0] send_to_element__val;
	localparam [5:0] __const__CMD_REARM = 6'd59;
	localparam [2:0] __const__num_fu_inports_at_update_msg = 3'd4;
	localparam [3:0] __const__num_routing_outports_at_update_msg = 4'd8;
	localparam [1:0] __const__CMD_CONFIG = 2'd3;
	localparam [4:0] __const__CMD_GLOBAL_REDUCE_ADD_RESPONSE = 5'd20;
	localparam [4:0] __const__CMD_GLOBAL_REDUCE_MUL_RESPONSE = 5'd21;
	localparam [4:0] __const__CMD_CONFIG_LOOP_LOWER = 5'd28;
	localparam [4:0] __const__CMD_CONFIG_LOOP_UPPER = 5'd29;
	localparam [4:0] __const__CMD_CONFIG_LOOP_STEP = 5'd30;
	localparam [5:0] __const__CMD_CONFIG_GEP_STRIDE = 6'd43;
	localparam [4:0] __const__CMD_UPDATE_COUNTER_SHADOW_VALUE = 5'd26;
	localparam [4:0] __const__CMD_RESET_LEAF_COUNTER = 5'd27;
	localparam [2:0] __const__CMD_CONFIG_PROLOGUE_FU = 3'd4;
	localparam [2:0] __const__CMD_CONFIG_PROLOGUE_FU_CROSSBAR = 3'd5;
	localparam [2:0] __const__CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR = 3'd6;
	localparam [0:0] __const__CMD_LAUNCH = 1'd0;
	localparam [1:0] __const__CMD_TERMINATE = 2'd2;
	localparam [2:0] __const__CMD_CONFIG_TOTAL_CTRL_COUNT = 3'd7;
	localparam [3:0] __const__CMD_CONFIG_COUNT_PER_ITER = 4'd8;
	localparam [3:0] __const__CMD_CONFIG_CTRL_LOWER_BOUND = 4'd9;
	localparam [4:0] __const__CMD_CONFIG_STREAMING_LD_START_ADDR = 5'd23;
	localparam [4:0] __const__CMD_CONFIG_STREAMING_LD_STRIDE = 5'd24;
	localparam [4:0] __const__CMD_CONFIG_STREAMING_LD_END_ADDR = 5'd25;
	localparam [0:0] __const__has_context_switch_at_update_msg = 1'd0;
	localparam [0:0] __const__CMD_PAUSE = 1'd1;
	localparam [4:0] __const__CMD_PRESERVE = 5'd22;
	localparam [3:0] __const__CMD_RESUME = 4'd15;
	localparam [4:0] __const__CMD_RECORD_PHI_ADDR = 5'd16;
	localparam [4:0] __const__num_tiles_at_update_send_pkt_to_controller = 5'd16;
	localparam [3:0] __const__CMD_COMPLETE = 4'd14;
	localparam [6:0] __const__OPT_START = 7'd0;
	localparam [2:0] __const__num_fu_inports_at_update_send_ctrl_msg = 3'd4;
	localparam [3:0] __const__num_routing_outports_at_update_send_ctrl_msg = 4'd8;
	localparam [6:0] __const__OPT_NAH = 7'd1;
	localparam [0:0] __const__has_context_switch_at_update_whether_we_can_iterate_ctrl = 1'd0;
	localparam [0:0] __const__has_context_switch_at_issue_complete = 1'd0;
	localparam [5:0] __const__ctrl_mem_size_at_update_raddr_and_fu_prologue = 6'd48;
	localparam [5:0] __const__ctrl_mem_size_at_update_prologue_outport = 6'd48;
	localparam [3:0] __const__num_routing_xbar_inports_at_update_prologue_outport = 4'd8;
	localparam [1:0] __const__num_fu_outports_at_update_prologue_outport = 2'd2;
	localparam [5:0] __const__ctrl_mem_size_at_update_prologue_reg = 6'd48;
	localparam [3:0] __const__num_routing_xbar_inports_at_update_prologue_reg = 4'd8;
	localparam [1:0] __const__num_fu_outports_at_update_prologue_reg = 2'd2;
	localparam [5:0] __const__ctrl_count_per_iter_at_update_ctrl_count_per_iter = 6'd48;
	localparam [5:0] __const__total_ctrl_steps_at_update_total_ctrl_steps = 6'd48;
	reg [5:0] ctrl_count_lower_bound;
	reg [5:0] ctrl_count_per_iter_val;
	reg [5:0] ctrl_count_upper_bound;
	reg [2:0] prologue_count_reg_fu [0:47];
	reg [2:0] prologue_count_reg_fu_crossbar [0:47][0:1];
	reg [2:0] prologue_count_reg_routing_crossbar [0:47][0:7];
	reg [0:0] sent_complete;
	reg [0:0] start_iterate_ctrl;
	reg [15:0] times;
	reg [15:0] total_ctrl_steps_val;
	wire [0:0] recv_from_element_queue__clear;
	wire [0:0] recv_from_element_queue__clk;
	wire [1:0] recv_from_element_queue__count;
	wire [0:0] recv_from_element_queue__reset;
	wire [172:0] recv_from_element_queue__recv__msg;
	wire [0:0] recv_from_element_queue__recv__rdy;
	wire [0:0] recv_from_element_queue__recv__val;
	wire [172:0] recv_from_element_queue__send__msg;
	reg [0:0] recv_from_element_queue__send__rdy;
	wire [0:0] recv_from_element_queue__send__val;
	PeQueue_NormalQueueWithClearRTL__17e45bee21dfb909 recv_from_element_queue(
		.clear(recv_from_element_queue__clear),
		.clk(recv_from_element_queue__clk),
		.count(recv_from_element_queue__count),
		.reset(recv_from_element_queue__reset),
		.recv__msg(recv_from_element_queue__recv__msg),
		.recv__rdy(recv_from_element_queue__recv__rdy),
		.recv__val(recv_from_element_queue__recv__val),
		.send__msg(recv_from_element_queue__send__msg),
		.send__rdy(recv_from_element_queue__send__rdy),
		.send__val(recv_from_element_queue__send__val)
	);
	wire [0:0] recv_pkt_from_controller_queue__clk;
	wire [1:0] recv_pkt_from_controller_queue__count;
	wire [0:0] recv_pkt_from_controller_queue__reset;
	wire [197:0] recv_pkt_from_controller_queue__recv__msg;
	wire [0:0] recv_pkt_from_controller_queue__recv__rdy;
	wire [0:0] recv_pkt_from_controller_queue__recv__val;
	wire [197:0] recv_pkt_from_controller_queue__send__msg;
	reg [0:0] recv_pkt_from_controller_queue__send__rdy;
	wire [0:0] recv_pkt_from_controller_queue__send__val;
	PeQueue_NormalQueueRTL__3f50370143e38621 recv_pkt_from_controller_queue(
		.clk(recv_pkt_from_controller_queue__clk),
		.count(recv_pkt_from_controller_queue__count),
		.reset(recv_pkt_from_controller_queue__reset),
		.recv__msg(recv_pkt_from_controller_queue__recv__msg),
		.recv__rdy(recv_pkt_from_controller_queue__recv__rdy),
		.recv__val(recv_pkt_from_controller_queue__recv__val),
		.send__msg(recv_pkt_from_controller_queue__send__msg),
		.send__rdy(recv_pkt_from_controller_queue__send__rdy),
		.send__val(recv_pkt_from_controller_queue__send__val)
	);
	wire [0:0] reg_file__clk;
	reg [5:0] reg_file__raddr;
	wire [110:0] reg_file__rdata;
	wire [0:0] reg_file__reset;
	reg [5:0] reg_file__waddr;
	reg [110:0] reg_file__wdata;
	reg [0:0] reg_file__wen;
	PeQueue_RegisterFile__82f517189341f536 reg_file(
		.clk(reg_file__clk),
		.raddr(reg_file__raddr),
		.rdata(reg_file__rdata),
		.reset(reg_file__reset),
		.waddr(reg_file__waddr),
		.wdata(reg_file__wdata),
		.wen(reg_file__wen)
	);
	reg [3:0] __tmpvar__update_prologue_reg_temp_routing_crossbar_in;
	reg [1:0] __tmpvar__update_prologue_reg_temp_fu_crossbar_in;
	always @(*) begin : retire_rearm
		if (_sv2v_0)
			;
		rearm = (recv_pkt_from_controller_queue__send__val & recv_pkt_from_controller_queue__send__rdy) & (recv_pkt_from_controller_queue__send__msg[172-:6] == __const__CMD_REARM);
	end
	always @(*) begin : update_ctrl_addr_outport
		if (_sv2v_0)
			;
		ctrl_addr_outport = reg_file__raddr[1'd0 * 6+:6];
	end
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	function automatic [2:0] sv2v_cast_3;
		input reg [2:0] inp;
		sv2v_cast_3 = inp;
	endfunction
	function automatic [5:0] sv2v_cast_6;
		input reg [5:0] inp;
		sv2v_cast_6 = inp;
	endfunction
	always @(*) begin : update_msg
		if (_sv2v_0)
			;
		recv_pkt_from_controller_queue__send__rdy = 1'd0;
		send_to_element__msg = 173'h00000000000000000000000000000000000000000000;
		send_to_element__val = 1'd0;
		reg_file__wen[1'd0+:1] = 1'd0;
		reg_file__waddr[1'd0 * 6+:6] = recv_pkt_from_controller_queue__send__msg[5-:6];
		reg_file__wdata[(1'd0 * 111) + 110-:7] = 7'd0;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_fu_inports_at_update_msg; i = i + 1'd1)
				begin
					reg_file__wdata[(1'd0 * 111) + (92 + (sv2v_cast_2(i) * 3))+:3] = 3'd0;
					reg_file__wdata[(1'd0 * 111) + (32 + (sv2v_cast_2(i) * 2))+:2] = recv_pkt_from_controller_queue__send__msg[38 + (sv2v_cast_2(i) * 2)+:2];
					reg_file__wdata[(1'd0 * 111) + (20 + (sv2v_cast_2(i) * 3))+:3] = recv_pkt_from_controller_queue__send__msg[26 + (sv2v_cast_2(i) * 3)+:3];
					reg_file__wdata[(1'd0 * 111) + (12 + (sv2v_cast_2(i) * 2))+:2] = recv_pkt_from_controller_queue__send__msg[18 + (sv2v_cast_2(i) * 2)+:2];
					reg_file__wdata[(1'd0 * 111) + (0 + (sv2v_cast_2(i) * 3))+:3] = recv_pkt_from_controller_queue__send__msg[6 + (sv2v_cast_2(i) * 3)+:3];
				end
		end
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_routing_outports_at_update_msg; i = i + 1'd1)
				begin
					reg_file__wdata[(1'd0 * 111) + (60 + (sv2v_cast_3(i) * 4))+:4] = 4'd0;
					reg_file__wdata[(1'd0 * 111) + (44 + (sv2v_cast_3(i) * 2))+:2] = 2'd0;
				end
		end
		reg_file__wdata[(1'd0 * 111) + 43-:3] = recv_pkt_from_controller_queue__send__msg[49-:3];
		reg_file__wdata[(1'd0 * 111) + 40] = 1'd0;
		if (recv_pkt_from_controller_queue__send__val & (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG))) begin
			reg_file__wen[1'd0+:1] = 1'd1;
			reg_file__waddr[1'd0 * 6+:6] = recv_pkt_from_controller_queue__send__msg[5-:6];
			reg_file__wdata[(1'd0 * 111) + 110-:7] = recv_pkt_from_controller_queue__send__msg[116-:7];
			begin : sv2v_autoblock_3
				reg [31:0] i;
				for (i = 1'd0; i < __const__num_fu_inports_at_update_msg; i = i + 1'd1)
					begin
						reg_file__wdata[(1'd0 * 111) + (92 + (sv2v_cast_2(i) * 3))+:3] = recv_pkt_from_controller_queue__send__msg[98 + (sv2v_cast_2(i) * 3)+:3];
						reg_file__wdata[(1'd0 * 111) + (32 + (sv2v_cast_2(i) * 2))+:2] = recv_pkt_from_controller_queue__send__msg[38 + (sv2v_cast_2(i) * 2)+:2];
						reg_file__wdata[(1'd0 * 111) + (20 + (sv2v_cast_2(i) * 3))+:3] = recv_pkt_from_controller_queue__send__msg[26 + (sv2v_cast_2(i) * 3)+:3];
						reg_file__wdata[(1'd0 * 111) + (12 + (sv2v_cast_2(i) * 2))+:2] = recv_pkt_from_controller_queue__send__msg[18 + (sv2v_cast_2(i) * 2)+:2];
						reg_file__wdata[(1'd0 * 111) + (0 + (sv2v_cast_2(i) * 3))+:3] = recv_pkt_from_controller_queue__send__msg[6 + (sv2v_cast_2(i) * 3)+:3];
					end
			end
			begin : sv2v_autoblock_4
				reg [31:0] i;
				for (i = 1'd0; i < __const__num_routing_outports_at_update_msg; i = i + 1'd1)
					begin
						reg_file__wdata[(1'd0 * 111) + (60 + (sv2v_cast_3(i) * 4))+:4] = recv_pkt_from_controller_queue__send__msg[66 + (sv2v_cast_3(i) * 4)+:4];
						reg_file__wdata[(1'd0 * 111) + (44 + (sv2v_cast_3(i) * 2))+:2] = recv_pkt_from_controller_queue__send__msg[50 + (sv2v_cast_3(i) * 2)+:2];
					end
			end
			reg_file__wdata[(1'd0 * 111) + 43-:3] = recv_pkt_from_controller_queue__send__msg[49-:3];
			reg_file__wdata[(1'd0 * 111) + 40] = recv_pkt_from_controller_queue__send__msg[46];
		end
		else if (recv_pkt_from_controller_queue__send__val & (((((((((recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_GLOBAL_REDUCE_ADD_RESPONSE)) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_GLOBAL_REDUCE_MUL_RESPONSE))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_LOOP_LOWER))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_LOOP_UPPER))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_LOOP_STEP))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == __const__CMD_CONFIG_GEP_STRIDE)) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_UPDATE_COUNTER_SHADOW_VALUE))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == __const__CMD_REARM)) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_RESET_LEAF_COUNTER)))) begin
			send_to_element__msg = recv_pkt_from_controller_queue__send__msg[172-:173];
			send_to_element__val = 1'd1;
		end
		if ((((((((((((((((((((((recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG)) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_PROLOGUE_FU))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_PROLOGUE_FU_CROSSBAR))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_LAUNCH))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_TERMINATE))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == __const__CMD_REARM)) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_TOTAL_CTRL_COUNT))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_COUNT_PER_ITER))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_CTRL_LOWER_BOUND))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_GLOBAL_REDUCE_ADD_RESPONSE))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_STREAMING_LD_START_ADDR))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_STREAMING_LD_STRIDE))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_STREAMING_LD_END_ADDR))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_GLOBAL_REDUCE_MUL_RESPONSE))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_LOOP_LOWER))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_LOOP_UPPER))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_LOOP_STEP))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == __const__CMD_CONFIG_GEP_STRIDE)) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_UPDATE_COUNTER_SHADOW_VALUE))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_RESET_LEAF_COUNTER))) | (__const__has_context_switch_at_update_msg & ((((recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_PAUSE)) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_PRESERVE))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_RESUME))) | (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_RECORD_PHI_ADDR)))))
			recv_pkt_from_controller_queue__send__rdy = 1'd1;
	end
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : update_prologue_outport
		if (_sv2v_0)
			;
		prologue_count_outport_fu = prologue_count_reg_fu[reg_file__raddr[1'd0 * 6+:6]];
		begin : sv2v_autoblock_5
			reg [31:0] addr;
			for (addr = 1'd0; addr < __const__ctrl_mem_size_at_update_prologue_outport; addr = addr + 1'd1)
				begin
					begin : sv2v_autoblock_6
						reg [31:0] i;
						for (i = 1'd0; i < __const__num_routing_xbar_inports_at_update_prologue_outport; i = i + 1'd1)
							prologue_count_outport_routing_crossbar[(((47 - sv2v_cast_6(addr)) * 8) + (7 - sv2v_cast_3(i))) * 3+:3] = prologue_count_reg_routing_crossbar[sv2v_cast_6(addr)][sv2v_cast_3(i)];
					end
					begin : sv2v_autoblock_7
						reg [31:0] i;
						for (i = 1'd0; i < __const__num_fu_outports_at_update_prologue_outport; i = i + 1'd1)
							prologue_count_outport_fu_crossbar[(((47 - sv2v_cast_6(addr)) * 2) + (1 - sv2v_cast_1(i))) * 3+:3] = prologue_count_reg_fu_crossbar[sv2v_cast_6(addr)][sv2v_cast_1(i)];
					end
				end
		end
	end
	always @(*) begin : update_send_ctrl_msg
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_8
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_fu_inports_at_update_send_ctrl_msg; i = i + 1'd1)
				begin
					send_ctrl__msg[92 + (sv2v_cast_2(i) * 3)+:3] = reg_file__rdata[(1'd0 * 111) + (92 + (sv2v_cast_2(i) * 3))+:3];
					send_ctrl__msg[32 + (sv2v_cast_2(i) * 2)+:2] = reg_file__rdata[(1'd0 * 111) + (32 + (sv2v_cast_2(i) * 2))+:2];
					send_ctrl__msg[20 + (sv2v_cast_2(i) * 3)+:3] = reg_file__rdata[(1'd0 * 111) + (20 + (sv2v_cast_2(i) * 3))+:3];
					send_ctrl__msg[12 + (sv2v_cast_2(i) * 2)+:2] = reg_file__rdata[(1'd0 * 111) + (12 + (sv2v_cast_2(i) * 2))+:2];
					send_ctrl__msg[0 + (sv2v_cast_2(i) * 3)+:3] = reg_file__rdata[(1'd0 * 111) + (0 + (sv2v_cast_2(i) * 3))+:3];
				end
		end
		begin : sv2v_autoblock_9
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_routing_outports_at_update_send_ctrl_msg; i = i + 1'd1)
				begin
					send_ctrl__msg[60 + (sv2v_cast_3(i) * 4)+:4] = reg_file__rdata[(1'd0 * 111) + (60 + (sv2v_cast_3(i) * 4))+:4];
					send_ctrl__msg[44 + (sv2v_cast_3(i) * 2)+:2] = reg_file__rdata[(1'd0 * 111) + (44 + (sv2v_cast_3(i) * 2))+:2];
				end
		end
		send_ctrl__msg[43-:3] = reg_file__rdata[(1'd0 * 111) + 43-:3];
		send_ctrl__msg[40] = reg_file__rdata[(1'd0 * 111) + 40];
		if (prologue_count_outport_fu != 3'd0)
			send_ctrl__msg[110-:7] = __const__OPT_NAH;
		else
			send_ctrl__msg[110-:7] = reg_file__rdata[(1'd0 * 111) + 110-:7];
	end
	always @(*) begin : update_send_ctrl_val
		if (_sv2v_0)
			;
		send_ctrl__val = 1'd0;
		if (start_iterate_ctrl == 1'd1) begin
			if (sent_complete)
				send_ctrl__val = 1'd0;
			else if (((total_ctrl_steps_val > 16'd0) & (times == total_ctrl_steps_val)) | (reg_file__rdata[(1'd0 * 111) + 110-:7] == __const__OPT_START))
				send_ctrl__val = 1'd0;
			else
				send_ctrl__val = 1'd1;
		end
		if (recv_pkt_from_controller_queue__send__val & ((recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_TERMINATE)) | rearm))
			send_ctrl__val = 1'd0;
	end
	always @(*) begin : update_send_pkt_to_controller
		if (_sv2v_0)
			;
		send_pkt_to_controller__val = 1'd0;
		send_pkt_to_controller__msg = {5'd0, __const__num_tiles_at_update_send_pkt_to_controller, 15'h0000, sv2v_cast_6(__const__CMD_COMPLETE), 167'h000000000000000000000000000000000000000000};
		recv_from_element_queue__send__rdy = 1'd0;
		if (start_iterate_ctrl & ~rearm) begin
			if (recv_from_element_queue__send__val & ~sent_complete) begin
				send_pkt_to_controller__msg = {tile_id, __const__num_tiles_at_update_send_pkt_to_controller, 15'h0000, recv_from_element_queue__send__msg};
				send_pkt_to_controller__val = 1'd1;
				recv_from_element_queue__send__rdy = send_pkt_to_controller__rdy;
			end
			else if (((total_ctrl_steps_val > 16'd0) & (times == total_ctrl_steps_val)) | (reg_file__rdata[(1'd0 * 111) + 110-:7] == __const__OPT_START)) begin
				if (((~sent_complete & (total_ctrl_steps_val > 16'd0)) & (times == total_ctrl_steps_val)) & start_iterate_ctrl) begin
					send_pkt_to_controller__msg = {tile_id, __const__num_tiles_at_update_send_pkt_to_controller, 15'h0000, sv2v_cast_6(__const__CMD_COMPLETE), 167'h000000000000000000000000000000000000000000};
					send_pkt_to_controller__val = 1'd1;
				end
			end
		end
	end
	always @(*) begin : update_upper_bound
		if (_sv2v_0)
			;
		ctrl_count_upper_bound = ctrl_count_lower_bound + ctrl_count_per_iter_val;
	end
	always @(posedge clk) begin : issue_complete
		if (reset | rearm)
			sent_complete <= 1'd0;
		else if ((send_pkt_to_controller__val & send_pkt_to_controller__rdy) & (send_pkt_to_controller__msg[172-:6] == sv2v_cast_6(__const__CMD_COMPLETE)))
			sent_complete <= 1'd1;
		else if (recv_pkt_from_controller_queue__send__val & ((recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_LAUNCH)) | (__const__has_context_switch_at_issue_complete & (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_RESUME)))))
			sent_complete <= 1'd0;
	end
	always @(posedge clk) begin : update_ctrl_count_per_iter
		if (reset)
			ctrl_count_per_iter_val <= 6'd48;
		else if (recv_pkt_from_controller_queue__send__val & (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_COUNT_PER_ITER)))
			ctrl_count_per_iter_val <= sv2v_cast_6(recv_pkt_from_controller_queue__send__msg[166-:32]);
	end
	always @(posedge clk) begin : update_lower_bound
		if (reset)
			ctrl_count_lower_bound <= 6'd0;
		else if (recv_pkt_from_controller_queue__send__val & (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_CTRL_LOWER_BOUND)))
			ctrl_count_lower_bound <= sv2v_cast_6(recv_pkt_from_controller_queue__send__msg[166-:32]);
	end
	always @(posedge clk) begin : update_prologue_reg
		if (reset) begin : sv2v_autoblock_10
			reg [31:0] addr;
			for (addr = 1'd0; addr < __const__ctrl_mem_size_at_update_prologue_reg; addr = addr + 1'd1)
				begin
					begin : sv2v_autoblock_11
						reg [31:0] i;
						for (i = 1'd0; i < __const__num_routing_xbar_inports_at_update_prologue_reg; i = i + 1'd1)
							prologue_count_reg_routing_crossbar[sv2v_cast_6(addr)][sv2v_cast_3(i)] <= 3'd0;
					end
					begin : sv2v_autoblock_12
						reg [31:0] i;
						for (i = 1'd0; i < __const__num_fu_outports_at_update_prologue_reg; i = i + 1'd1)
							prologue_count_reg_fu_crossbar[sv2v_cast_6(addr)][sv2v_cast_1(i)] <= 3'd0;
					end
				end
		end
		else if (recv_pkt_from_controller_queue__send__val & (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR))) begin
			__tmpvar__update_prologue_reg_temp_routing_crossbar_in = recv_pkt_from_controller_queue__send__msg[66 + (3'd0 * 4)+:4];
			if (__tmpvar__update_prologue_reg_temp_routing_crossbar_in > 4'd0)
				prologue_count_reg_routing_crossbar[recv_pkt_from_controller_queue__send__msg[5-:6]][sv2v_cast_3(__tmpvar__update_prologue_reg_temp_routing_crossbar_in - 4'd1)] <= sv2v_cast_3(recv_pkt_from_controller_queue__send__msg[166-:32]);
		end
		else if (recv_pkt_from_controller_queue__send__val & (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_PROLOGUE_FU_CROSSBAR))) begin
			__tmpvar__update_prologue_reg_temp_fu_crossbar_in = recv_pkt_from_controller_queue__send__msg[50 + (3'd0 * 2)+:2];
			prologue_count_reg_fu_crossbar[recv_pkt_from_controller_queue__send__msg[5-:6]][sv2v_cast_1(__tmpvar__update_prologue_reg_temp_fu_crossbar_in)] <= sv2v_cast_3(recv_pkt_from_controller_queue__send__msg[166-:32]);
		end
	end
	always @(posedge clk) begin : update_raddr_and_fu_prologue
		if (reset) begin
			times <= 16'd0;
			reg_file__raddr[1'd0 * 6+:6] <= 6'd0;
			begin : sv2v_autoblock_13
				reg [31:0] i;
				for (i = 1'd0; i < __const__ctrl_mem_size_at_update_raddr_and_fu_prologue; i = i + 1'd1)
					prologue_count_reg_fu[sv2v_cast_6(i)] <= 3'd0;
			end
		end
		else if (rearm) begin
			times <= 16'd0;
			reg_file__raddr[1'd0 * 6+:6] <= ctrl_count_lower_bound;
			begin : sv2v_autoblock_14
				reg [31:0] i;
				for (i = 1'd0; i < __const__ctrl_mem_size_at_update_raddr_and_fu_prologue; i = i + 1'd1)
					prologue_count_reg_fu[sv2v_cast_6(i)] <= 3'd0;
			end
		end
		else if (recv_pkt_from_controller_queue__send__val & (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_CTRL_LOWER_BOUND)))
			reg_file__raddr[1'd0 * 6+:6] <= sv2v_cast_6(recv_pkt_from_controller_queue__send__msg[166-:32]);
		else if (recv_pkt_from_controller_queue__send__val & (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_TERMINATE)))
			times <= 16'd0;
		else begin
			if (recv_pkt_from_controller_queue__send__val & (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_PROLOGUE_FU)))
				prologue_count_reg_fu[recv_pkt_from_controller_queue__send__msg[5-:6]] <= sv2v_cast_3(recv_pkt_from_controller_queue__send__msg[166-:32]);
			if (start_iterate_ctrl == 1'd1) begin
				if ((((total_ctrl_steps_val == 16'd0) | (times < total_ctrl_steps_val)) & send_ctrl__rdy) & send_ctrl__val)
					times <= times + 16'd1;
				if (send_ctrl__rdy & send_ctrl__val) begin
					if (reg_file__raddr[1'd0 * 6+:6] == (ctrl_count_upper_bound - 6'd1))
						reg_file__raddr[1'd0 * 6+:6] <= ctrl_count_lower_bound;
					else
						reg_file__raddr[1'd0 * 6+:6] <= reg_file__raddr[1'd0 * 6+:6] + 6'd1;
					if (prologue_count_reg_fu[reg_file__raddr[1'd0 * 6+:6]] > 3'd0)
						prologue_count_reg_fu[reg_file__raddr[1'd0 * 6+:6]] <= prologue_count_reg_fu[reg_file__raddr[1'd0 * 6+:6]] - 3'd1;
				end
			end
		end
	end
	function automatic [15:0] sv2v_cast_16;
		input reg [15:0] inp;
		sv2v_cast_16 = inp;
	endfunction
	always @(posedge clk) begin : update_total_ctrl_steps
		if (reset)
			total_ctrl_steps_val <= 16'd48;
		else if (recv_pkt_from_controller_queue__send__val & (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_TOTAL_CTRL_COUNT)))
			total_ctrl_steps_val <= sv2v_cast_16(recv_pkt_from_controller_queue__send__msg[166-:32]);
	end
	always @(posedge clk) begin : update_whether_we_can_iterate_ctrl
		if (reset | rearm)
			start_iterate_ctrl <= 1'd0;
		else if (recv_pkt_from_controller_queue__send__val) begin
			if ((recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_LAUNCH)) | (__const__has_context_switch_at_update_whether_we_can_iterate_ctrl & (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_RESUME))))
				start_iterate_ctrl <= 1'd1;
			else if (recv_pkt_from_controller_queue__send__msg[172-:6] == sv2v_cast_6(__const__CMD_TERMINATE))
				start_iterate_ctrl <= 1'd0;
		end
	end
	assign reg_file__clk = clk;
	assign reg_file__reset = reset;
	assign recv_pkt_from_controller_queue__clk = clk;
	assign recv_pkt_from_controller_queue__reset = reset;
	assign recv_from_element_queue__clk = clk;
	assign recv_from_element_queue__reset = reset;
	assign recv_pkt_from_controller_queue__recv__msg = recv_pkt_from_controller__msg;
	assign recv_pkt_from_controller__rdy = recv_pkt_from_controller_queue__recv__rdy;
	assign recv_pkt_from_controller_queue__recv__val = recv_pkt_from_controller__val;
	assign recv_from_element_queue__recv__msg = recv_from_element__msg;
	assign recv_from_element__rdy = recv_from_element_queue__recv__rdy;
	assign recv_from_element_queue__recv__val = recv_from_element__val;
	assign recv_from_element_queue__clear = rearm;
	initial _sv2v_0 = 0;
endmodule
module AdderRTL__00ec249fa9189fd7 (
	clear,
	clk,
	ctrl_addr_inport,
	reset,
	from_mem_rdata__msg,
	from_mem_rdata__rdy,
	from_mem_rdata__val,
	recv_const__msg,
	recv_const__rdy,
	recv_const__val,
	recv_from_ctrl_mem__msg,
	recv_from_ctrl_mem__rdy,
	recv_from_ctrl_mem__val,
	recv_in__msg,
	recv_in__rdy,
	recv_in__val,
	recv_opt__msg,
	recv_opt__rdy,
	recv_opt__val,
	send_out__msg,
	send_out__rdy,
	send_out__val,
	send_to_ctrl_mem__msg,
	send_to_ctrl_mem__rdy,
	send_to_ctrl_mem__val,
	to_mem_raddr__msg,
	to_mem_raddr__rdy,
	to_mem_raddr__val,
	to_mem_waddr__msg,
	to_mem_waddr__rdy,
	to_mem_waddr__val,
	to_mem_wdata__msg,
	to_mem_wdata__rdy,
	to_mem_wdata__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [5:0] ctrl_addr_inport;
	input wire [0:0] reset;
	input wire [34:0] from_mem_rdata__msg;
	output reg [0:0] from_mem_rdata__rdy;
	input wire [0:0] from_mem_rdata__val;
	input wire [34:0] recv_const__msg;
	output reg [0:0] recv_const__rdy;
	input wire [0:0] recv_const__val;
	input wire [172:0] recv_from_ctrl_mem__msg;
	output reg [0:0] recv_from_ctrl_mem__rdy;
	input wire [0:0] recv_from_ctrl_mem__val;
	input wire [139:0] recv_in__msg;
	output reg [3:0] recv_in__rdy;
	input wire [3:0] recv_in__val;
	input wire [110:0] recv_opt__msg;
	output reg [0:0] recv_opt__rdy;
	input wire [0:0] recv_opt__val;
	output reg [69:0] send_out__msg;
	input wire [1:0] send_out__rdy;
	output reg [1:0] send_out__val;
	output reg [172:0] send_to_ctrl_mem__msg;
	input wire [0:0] send_to_ctrl_mem__rdy;
	output reg [0:0] send_to_ctrl_mem__val;
	output reg [14:0] to_mem_raddr__msg;
	input wire [0:0] to_mem_raddr__rdy;
	output reg [0:0] to_mem_raddr__val;
	output reg [14:0] to_mem_waddr__msg;
	input wire [0:0] to_mem_waddr__rdy;
	output reg [0:0] to_mem_waddr__val;
	output reg [34:0] to_mem_wdata__msg;
	input wire [0:0] to_mem_wdata__rdy;
	output reg [0:0] to_mem_wdata__val;
	localparam [34:0] const_zero = 35'h000000000;
	localparam [6:0] __const__OPT_START = 7'd0;
	localparam [0:0] __const__latency_at_proceed_latency = 1'd1;
	localparam [2:0] __const__num_inports_at_comb_logic = 3'd4;
	localparam [1:0] __const__num_outports_at_comb_logic = 2'd2;
	localparam [6:0] __const__OPT_ADD = 7'd2;
	localparam [6:0] __const__OPT_ADD_CONST = 7'd25;
	localparam [6:0] __const__OPT_INC = 7'd3;
	localparam [6:0] __const__OPT_SUB = 7'd4;
	localparam [6:0] __const__OPT_SUB_CONST = 7'd36;
	localparam [6:0] __const__OPT_PAS = 7'd31;
	reg [2:0] in0;
	wire [1:0] in0_idx;
	reg [2:0] in1;
	wire [1:0] in1_idx;
	reg [0:0] latency;
	reg [0:0] reached_vector_factor;
	reg [0:0] recv_all_val;
	reg [7:0] vector_factor_counter;
	wire [2:0] vector_factor_power;
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : comb_logic
		if (_sv2v_0)
			;
		recv_all_val = 1'd0;
		in0 = 3'd0;
		in1 = 3'd0;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_comb_logic; i = i + 1'd1)
				recv_in__rdy[3 - sv2v_cast_2(i)+:1] = 1'd0;
		end
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_comb_logic; i = i + 1'd1)
				begin
					send_out__val[1 - sv2v_cast_1(i)+:1] = 1'd0;
					send_out__msg[(1 - sv2v_cast_1(i)) * 35+:35] = 35'h000000000;
				end
		end
		recv_const__rdy = 1'd0;
		recv_opt__rdy = 1'd0;
		send_to_ctrl_mem__val = 1'd0;
		send_to_ctrl_mem__msg = 173'h00000000000000000000000000000000000000000000;
		recv_from_ctrl_mem__rdy = 1'd0;
		if (recv_opt__val) begin
			if (recv_opt__msg[92 + (2'd0 * 3)+:3] != 3'd0)
				in0 = recv_opt__msg[92 + (2'd0 * 3)+:3] - 3'd1;
			if (recv_opt__msg[92 + (2'd1 * 3)+:3] != 3'd0)
				in1 = recv_opt__msg[92 + (2'd1 * 3)+:3] - 3'd1;
		end
		if (recv_opt__val) begin
			if (recv_opt__msg[110-:7] == __const__OPT_ADD) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32] + recv_in__msg[((3 - in1_idx) * 35) + 34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_in__msg[((3 - in1_idx) * 35) + 2]) & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_in__val[3 - in1_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in1_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_ADD_CONST) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32] + recv_const__msg[34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_const__msg[2]) & reached_vector_factor;
				recv_const__rdy = send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_const__val;
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_const__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_INC) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32] + 32'd1;
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = recv_in__msg[((3 - in0_idx) * 35) + 2] & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_SUB) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32] - recv_in__msg[((3 - in1_idx) * 35) + 34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_in__msg[((3 - in1_idx) * 35) + 2]) & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_in__val[3 - in1_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in1_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_SUB_CONST) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32] - recv_const__msg[34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_const__msg[2]) & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_const__val;
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_const__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_PAS) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = recv_in__msg[((3 - in0_idx) * 35) + 2] & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else begin
				begin : sv2v_autoblock_3
					reg [31:0] j;
					for (j = 1'd0; j < __const__num_outports_at_comb_logic; j = j + 1'd1)
						send_out__val[1 - sv2v_cast_1(j)+:1] = 1'd0;
				end
				recv_opt__rdy = 1'd0;
				recv_in__rdy[3 - in0_idx+:1] = 1'd0;
				recv_in__rdy[3 - in1_idx+:1] = 1'd0;
			end
		end
	end
	always @(*) begin : update_mem
		if (_sv2v_0)
			;
		to_mem_waddr__val = 1'd0;
		to_mem_wdata__val = 1'd0;
		to_mem_wdata__msg = const_zero;
		to_mem_waddr__msg = 15'd0;
		to_mem_raddr__msg = 15'd0;
		to_mem_raddr__val = 1'd0;
		from_mem_rdata__rdy = 1'd0;
	end
	always @(*) begin : update_reached_vector_factor
		if (_sv2v_0)
			;
		reached_vector_factor = 1'd0;
		if (recv_opt__val & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) >= (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
			reached_vector_factor = 1'd1;
	end
	always @(posedge clk) begin : proceed_latency
		if ((reset | clear) | (recv_opt__msg[110-:7] == __const__OPT_START))
			latency <= 1'd0;
		else if (latency == (__const__latency_at_proceed_latency - 1'd1))
			latency <= 1'd0;
		else
			latency <= latency + 1'd1;
	end
	always @(posedge clk) begin : update_vector_factor_counter
		if (reset | clear)
			vector_factor_counter <= 8'd0;
		else if (recv_opt__val) begin
			if (recv_opt__msg[40] & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) < (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
				vector_factor_counter <= vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power});
			else if (recv_opt__msg[40] & reached_vector_factor)
				vector_factor_counter <= 8'd0;
		end
	end
	assign vector_factor_power = 3'd0;
	assign in0_idx = in0[1:0];
	assign in1_idx = in1[1:0];
	initial _sv2v_0 = 0;
endmodule
module RetRTL__00ec249fa9189fd7 (
	clear,
	clk,
	ctrl_addr_inport,
	reset,
	from_mem_rdata__msg,
	from_mem_rdata__rdy,
	from_mem_rdata__val,
	recv_const__msg,
	recv_const__rdy,
	recv_const__val,
	recv_from_ctrl_mem__msg,
	recv_from_ctrl_mem__rdy,
	recv_from_ctrl_mem__val,
	recv_in__msg,
	recv_in__rdy,
	recv_in__val,
	recv_opt__msg,
	recv_opt__rdy,
	recv_opt__val,
	send_out__msg,
	send_out__rdy,
	send_out__val,
	send_to_ctrl_mem__msg,
	send_to_ctrl_mem__rdy,
	send_to_ctrl_mem__val,
	to_mem_raddr__msg,
	to_mem_raddr__rdy,
	to_mem_raddr__val,
	to_mem_waddr__msg,
	to_mem_waddr__rdy,
	to_mem_waddr__val,
	to_mem_wdata__msg,
	to_mem_wdata__rdy,
	to_mem_wdata__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [5:0] ctrl_addr_inport;
	input wire [0:0] reset;
	input wire [34:0] from_mem_rdata__msg;
	output reg [0:0] from_mem_rdata__rdy;
	input wire [0:0] from_mem_rdata__val;
	input wire [34:0] recv_const__msg;
	output reg [0:0] recv_const__rdy;
	input wire [0:0] recv_const__val;
	input wire [172:0] recv_from_ctrl_mem__msg;
	output reg [0:0] recv_from_ctrl_mem__rdy;
	input wire [0:0] recv_from_ctrl_mem__val;
	input wire [139:0] recv_in__msg;
	output reg [3:0] recv_in__rdy;
	input wire [3:0] recv_in__val;
	input wire [110:0] recv_opt__msg;
	output reg [0:0] recv_opt__rdy;
	input wire [0:0] recv_opt__val;
	output reg [69:0] send_out__msg;
	input wire [1:0] send_out__rdy;
	output reg [1:0] send_out__val;
	output reg [172:0] send_to_ctrl_mem__msg;
	input wire [0:0] send_to_ctrl_mem__rdy;
	output reg [0:0] send_to_ctrl_mem__val;
	output reg [14:0] to_mem_raddr__msg;
	input wire [0:0] to_mem_raddr__rdy;
	output reg [0:0] to_mem_raddr__val;
	output reg [14:0] to_mem_waddr__msg;
	input wire [0:0] to_mem_waddr__rdy;
	output reg [0:0] to_mem_waddr__val;
	output reg [34:0] to_mem_wdata__msg;
	input wire [0:0] to_mem_wdata__rdy;
	output reg [0:0] to_mem_wdata__val;
	localparam [34:0] const_zero = 35'h000000000;
	localparam [6:0] __const__OPT_START = 7'd0;
	localparam [0:0] __const__latency_at_proceed_latency = 1'd1;
	localparam [2:0] __const__num_inports_at_comb_logic = 3'd4;
	localparam [1:0] __const__num_outports_at_comb_logic = 2'd2;
	localparam [6:0] __const__OPT_RET = 7'd35;
	localparam [3:0] __const__CMD_COMPLETE = 4'd14;
	localparam [6:0] __const__OPT_RET_VOID = 7'd64;
	localparam [6:0] __const__ctrl_mem_size_at_update_already_done = 7'd64;
	reg [0:0] already_done [0:63];
	reg [2:0] in0;
	wire [1:0] in0_idx;
	reg [0:0] latency;
	reg [0:0] reached_vector_factor;
	reg [0:0] recv_all_val;
	reg [7:0] vector_factor_counter;
	wire [2:0] vector_factor_power;
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	function automatic [5:0] sv2v_cast_6;
		input reg [5:0] inp;
		sv2v_cast_6 = inp;
	endfunction
	always @(*) begin : comb_logic
		if (_sv2v_0)
			;
		recv_all_val = 1'd0;
		in0 = 3'd0;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_comb_logic; i = i + 1'd1)
				recv_in__rdy[3 - sv2v_cast_2(i)+:1] = 1'd0;
		end
		begin : sv2v_autoblock_2
			reg [31:0] j;
			for (j = 1'd0; j < __const__num_outports_at_comb_logic; j = j + 1'd1)
				begin
					send_out__val[1 - sv2v_cast_1(j)+:1] = 1'd0;
					send_out__msg[(1 - sv2v_cast_1(j)) * 35+:35] = 35'h000000000;
				end
		end
		send_to_ctrl_mem__val = 1'd0;
		send_to_ctrl_mem__msg = 173'h00000000000000000000000000000000000000000000;
		recv_from_ctrl_mem__rdy = 1'd0;
		recv_const__rdy = 1'd0;
		recv_opt__rdy = 1'd0;
		if (recv_opt__val) begin
			if (recv_opt__msg[92 + (2'd0 * 3)+:3] != 3'd0)
				in0 = recv_opt__msg[92 + (2'd0 * 3)+:3] - 3'd1;
		end
		if (recv_opt__val) begin
			if (recv_opt__msg[110-:7] == __const__OPT_RET) begin
				recv_all_val = recv_in__val[3 - in0_idx+:1];
				if (already_done[ctrl_addr_inport]) begin
					recv_in__rdy[3 - in0_idx+:1] = recv_all_val;
					recv_opt__rdy = recv_all_val;
				end
				else if (recv_in__msg[((3 - in0_idx) * 35) + 2]) begin
					send_to_ctrl_mem__val = recv_all_val & reached_vector_factor;
					send_to_ctrl_mem__msg = {sv2v_cast_6(__const__CMD_COMPLETE), recv_in__msg[(3 - in0_idx) * 35+:35], 15'd0, recv_opt__msg, 6'd0};
					recv_in__rdy[3 - in0_idx+:1] = (recv_all_val & reached_vector_factor) & send_to_ctrl_mem__rdy;
					recv_opt__rdy = (recv_all_val & reached_vector_factor) & send_to_ctrl_mem__rdy;
				end
				else begin
					recv_in__rdy[3 - in0_idx+:1] = recv_all_val & reached_vector_factor;
					recv_opt__rdy = recv_all_val & reached_vector_factor;
				end
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_RET_VOID) begin
				recv_all_val = recv_in__val[3 - in0_idx+:1];
				if (already_done[ctrl_addr_inport]) begin
					recv_in__rdy[3 - in0_idx+:1] = recv_all_val;
					recv_opt__rdy = recv_all_val;
				end
				else if (recv_in__msg[((3 - in0_idx) * 35) + 2]) begin
					send_to_ctrl_mem__val = recv_all_val & reached_vector_factor;
					send_to_ctrl_mem__msg = {sv2v_cast_6(__const__CMD_COMPLETE), 50'h0000000000000, recv_opt__msg, 6'd0};
					recv_in__rdy[3 - in0_idx+:1] = (recv_all_val & reached_vector_factor) & send_to_ctrl_mem__rdy;
					recv_opt__rdy = (recv_all_val & reached_vector_factor) & send_to_ctrl_mem__rdy;
				end
				else begin
					recv_in__rdy[3 - in0_idx+:1] = recv_all_val & reached_vector_factor;
					recv_opt__rdy = recv_all_val & reached_vector_factor;
				end
			end
		end
	end
	always @(*) begin : update_mem
		if (_sv2v_0)
			;
		to_mem_waddr__val = 1'd0;
		to_mem_wdata__val = 1'd0;
		to_mem_wdata__msg = const_zero;
		to_mem_waddr__msg = 15'd0;
		to_mem_raddr__msg = 15'd0;
		to_mem_raddr__val = 1'd0;
		from_mem_rdata__rdy = 1'd0;
	end
	always @(*) begin : update_reached_vector_factor
		if (_sv2v_0)
			;
		reached_vector_factor = 1'd0;
		if (recv_opt__val & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) >= (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
			reached_vector_factor = 1'd1;
	end
	always @(posedge clk) begin : proceed_latency
		if ((reset | clear) | (recv_opt__msg[110-:7] == __const__OPT_START))
			latency <= 1'd0;
		else if (latency == (__const__latency_at_proceed_latency - 1'd1))
			latency <= 1'd0;
		else
			latency <= latency + 1'd1;
	end
	always @(posedge clk) begin : update_already_done
		if (reset | clear) begin : sv2v_autoblock_3
			reg [31:0] i;
			for (i = 1'd0; i < __const__ctrl_mem_size_at_update_already_done; i = i + 1'd1)
				already_done[sv2v_cast_6(i)] <= 1'd0;
		end
		else if ((((((recv_opt__val & ((recv_opt__msg[110-:7] == __const__OPT_RET) | (recv_opt__msg[110-:7] == __const__OPT_RET_VOID))) & ~already_done[ctrl_addr_inport]) & recv_all_val) & recv_in__msg[((3 - in0_idx) * 35) + 2]) & send_to_ctrl_mem__val) & send_to_ctrl_mem__rdy) begin : sv2v_autoblock_4
			reg [31:0] i;
			for (i = 1'd0; i < __const__ctrl_mem_size_at_update_already_done; i = i + 1'd1)
				if (sv2v_cast_6(i) == ctrl_addr_inport)
					already_done[sv2v_cast_6(i)] <= 1'd1;
				else
					already_done[sv2v_cast_6(i)] <= already_done[sv2v_cast_6(i)];
		end
		else begin : sv2v_autoblock_5
			reg [31:0] i;
			for (i = 1'd0; i < __const__ctrl_mem_size_at_update_already_done; i = i + 1'd1)
				already_done[sv2v_cast_6(i)] <= already_done[sv2v_cast_6(i)];
		end
	end
	always @(posedge clk) begin : update_vector_factor_counter
		if (reset | clear)
			vector_factor_counter <= 8'd0;
		else if (recv_opt__val) begin
			if (recv_opt__msg[40] & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) < (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
				vector_factor_counter <= vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power});
			else if (recv_opt__msg[40] & reached_vector_factor)
				vector_factor_counter <= 8'd0;
		end
	end
	assign vector_factor_power = 3'd0;
	assign in0_idx = in0[1:0];
	initial _sv2v_0 = 0;
endmodule
module GepRTL__00ec249fa9189fd7 (
	clear,
	clk,
	ctrl_addr_inport,
	reset,
	from_mem_rdata__msg,
	from_mem_rdata__rdy,
	from_mem_rdata__val,
	recv_const__msg,
	recv_const__rdy,
	recv_const__val,
	recv_from_ctrl_mem__msg,
	recv_from_ctrl_mem__rdy,
	recv_from_ctrl_mem__val,
	recv_in__msg,
	recv_in__rdy,
	recv_in__val,
	recv_opt__msg,
	recv_opt__rdy,
	recv_opt__val,
	send_out__msg,
	send_out__rdy,
	send_out__val,
	send_to_ctrl_mem__msg,
	send_to_ctrl_mem__rdy,
	send_to_ctrl_mem__val,
	to_mem_raddr__msg,
	to_mem_raddr__rdy,
	to_mem_raddr__val,
	to_mem_waddr__msg,
	to_mem_waddr__rdy,
	to_mem_waddr__val,
	to_mem_wdata__msg,
	to_mem_wdata__rdy,
	to_mem_wdata__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [5:0] ctrl_addr_inport;
	input wire [0:0] reset;
	input wire [34:0] from_mem_rdata__msg;
	output reg [0:0] from_mem_rdata__rdy;
	input wire [0:0] from_mem_rdata__val;
	input wire [34:0] recv_const__msg;
	output reg [0:0] recv_const__rdy;
	input wire [0:0] recv_const__val;
	input wire [172:0] recv_from_ctrl_mem__msg;
	output reg [0:0] recv_from_ctrl_mem__rdy;
	input wire [0:0] recv_from_ctrl_mem__val;
	input wire [139:0] recv_in__msg;
	output reg [3:0] recv_in__rdy;
	input wire [3:0] recv_in__val;
	input wire [110:0] recv_opt__msg;
	output reg [0:0] recv_opt__rdy;
	input wire [0:0] recv_opt__val;
	output reg [69:0] send_out__msg;
	input wire [1:0] send_out__rdy;
	output reg [1:0] send_out__val;
	output reg [172:0] send_to_ctrl_mem__msg;
	input wire [0:0] send_to_ctrl_mem__rdy;
	output reg [0:0] send_to_ctrl_mem__val;
	output reg [14:0] to_mem_raddr__msg;
	input wire [0:0] to_mem_raddr__rdy;
	output reg [0:0] to_mem_raddr__val;
	output reg [14:0] to_mem_waddr__msg;
	input wire [0:0] to_mem_waddr__rdy;
	output reg [0:0] to_mem_waddr__val;
	output reg [34:0] to_mem_wdata__msg;
	input wire [0:0] to_mem_wdata__rdy;
	output reg [0:0] to_mem_wdata__val;
	localparam [34:0] const_zero = 35'h000000000;
	localparam [6:0] __const__OPT_START = 7'd0;
	localparam [0:0] __const__latency_at_proceed_latency = 1'd1;
	localparam [2:0] __const__num_inports_at_comb_logic = 3'd4;
	localparam [1:0] __const__num_outports_at_comb_logic = 2'd2;
	localparam [6:0] __const__OPT_GEP = 7'd92;
	localparam [6:0] __const__OPT_GEP_CONST = 7'd93;
	localparam [6:0] __const__OPT_GEP_2D = 7'd94;
	localparam [6:0] __const__OPT_GEP_2D_CONST = 7'd95;
	localparam [5:0] __const__CMD_CONFIG_GEP_STRIDE = 6'd43;
	reg [2:0] in0;
	wire [1:0] in0_idx;
	reg [2:0] in1;
	wire [1:0] in1_idx;
	reg [2:0] in2;
	wire [1:0] in2_idx;
	reg [0:0] latency;
	reg [0:0] reached_vector_factor;
	reg [0:0] recv_all_val;
	reg [34:0] stride;
	reg [7:0] vector_factor_counter;
	wire [2:0] vector_factor_power;
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : comb_logic
		if (_sv2v_0)
			;
		recv_all_val = 1'd0;
		in0 = 3'd0;
		in1 = 3'd0;
		in2 = 3'd0;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_comb_logic; i = i + 1'd1)
				recv_in__rdy[3 - sv2v_cast_2(i)+:1] = 1'd0;
		end
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_comb_logic; i = i + 1'd1)
				begin
					send_out__val[1 - sv2v_cast_1(i)+:1] = 1'd0;
					send_out__msg[(1 - sv2v_cast_1(i)) * 35+:35] = 35'h000000000;
				end
		end
		recv_const__rdy = 1'd0;
		recv_opt__rdy = 1'd0;
		send_to_ctrl_mem__val = 1'd0;
		send_to_ctrl_mem__msg = 173'h00000000000000000000000000000000000000000000;
		recv_from_ctrl_mem__rdy = 1'd0;
		if (recv_from_ctrl_mem__val)
			recv_from_ctrl_mem__rdy = 1'd1;
		if (recv_opt__val) begin
			if (recv_opt__msg[92 + (2'd0 * 3)+:3] != 3'd0)
				in0 = recv_opt__msg[92 + (2'd0 * 3)+:3] - 3'd1;
			if (recv_opt__msg[92 + (2'd1 * 3)+:3] != 3'd0)
				in1 = recv_opt__msg[92 + (2'd1 * 3)+:3] - 3'd1;
			if (recv_opt__msg[92 + (2'd2 * 3)+:3] != 3'd0)
				in2 = recv_opt__msg[92 + (2'd2 * 3)+:3] - 3'd1;
		end
		if (recv_opt__val) begin
			if (recv_opt__msg[110-:7] == __const__OPT_GEP) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32] + recv_in__msg[((3 - in1_idx) * 35) + 34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_in__msg[((3 - in1_idx) * 35) + 2]) & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_in__val[3 - in1_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in1_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_GEP_CONST) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_const__msg[34-:32] + recv_in__msg[((3 - in0_idx) * 35) + 34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = recv_in__msg[((3 - in0_idx) * 35) + 2] & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_const__val;
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_const__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_GEP_2D) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = (recv_in__msg[((3 - in0_idx) * 35) + 34-:32] + (recv_in__msg[((3 - in1_idx) * 35) + 34-:32] * stride[34-:32])) + recv_in__msg[((3 - in2_idx) * 35) + 34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = ((recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_in__msg[((3 - in1_idx) * 35) + 2]) & recv_in__msg[((3 - in2_idx) * 35) + 2]) & reached_vector_factor;
				recv_all_val = (recv_in__val[3 - in0_idx+:1] & recv_in__val[3 - in1_idx+:1]) & recv_in__val[3 - in2_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in1_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in2_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_GEP_2D_CONST) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = (recv_const__msg[34-:32] + (recv_in__msg[((3 - in0_idx) * 35) + 34-:32] * stride[34-:32])) + recv_in__msg[((3 - in1_idx) * 35) + 34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_in__msg[((3 - in1_idx) * 35) + 2]) & reached_vector_factor;
				recv_all_val = (recv_in__val[3 - in0_idx+:1] & recv_in__val[3 - in1_idx+:1]) & recv_const__val;
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in1_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_const__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else begin
				begin : sv2v_autoblock_3
					reg [31:0] j;
					for (j = 1'd0; j < __const__num_outports_at_comb_logic; j = j + 1'd1)
						send_out__val[1 - sv2v_cast_1(j)+:1] = 1'd0;
				end
				recv_opt__rdy = 1'd0;
				recv_in__rdy[3 - in0_idx+:1] = 1'd0;
				recv_in__rdy[3 - in1_idx+:1] = 1'd0;
			end
		end
	end
	always @(*) begin : update_mem
		if (_sv2v_0)
			;
		to_mem_waddr__val = 1'd0;
		to_mem_wdata__val = 1'd0;
		to_mem_wdata__msg = const_zero;
		to_mem_waddr__msg = 15'd0;
		to_mem_raddr__msg = 15'd0;
		to_mem_raddr__val = 1'd0;
		from_mem_rdata__rdy = 1'd0;
	end
	always @(*) begin : update_reached_vector_factor
		if (_sv2v_0)
			;
		reached_vector_factor = 1'd0;
		if (recv_opt__val & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) >= (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
			reached_vector_factor = 1'd1;
	end
	always @(posedge clk) begin : proceed_latency
		if ((reset | clear) | (recv_opt__msg[110-:7] == __const__OPT_START))
			latency <= 1'd0;
		else if (latency == (__const__latency_at_proceed_latency - 1'd1))
			latency <= 1'd0;
		else
			latency <= latency + 1'd1;
	end
	always @(posedge clk) begin : update_stride
		if (reset)
			stride <= 35'h000000000;
		else if (recv_from_ctrl_mem__val & (recv_from_ctrl_mem__msg[172-:6] == __const__CMD_CONFIG_GEP_STRIDE))
			stride <= recv_from_ctrl_mem__msg[166-:35];
	end
	always @(posedge clk) begin : update_vector_factor_counter
		if (reset | clear)
			vector_factor_counter <= 8'd0;
		else if (recv_opt__val) begin
			if (recv_opt__msg[40] & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) < (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
				vector_factor_counter <= vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power});
			else if (recv_opt__msg[40] & reached_vector_factor)
				vector_factor_counter <= 8'd0;
		end
	end
	assign vector_factor_power = 3'd0;
	assign in0_idx = in0[1:0];
	assign in1_idx = in1[1:0];
	assign in2_idx = in2[1:0];
	initial _sv2v_0 = 0;
endmodule
module NahRTL__00ec249fa9189fd7 (
	clear,
	clk,
	ctrl_addr_inport,
	reset,
	from_mem_rdata__msg,
	from_mem_rdata__rdy,
	from_mem_rdata__val,
	recv_const__msg,
	recv_const__rdy,
	recv_const__val,
	recv_from_ctrl_mem__msg,
	recv_from_ctrl_mem__rdy,
	recv_from_ctrl_mem__val,
	recv_in__msg,
	recv_in__rdy,
	recv_in__val,
	recv_opt__msg,
	recv_opt__rdy,
	recv_opt__val,
	send_out__msg,
	send_out__rdy,
	send_out__val,
	send_to_ctrl_mem__msg,
	send_to_ctrl_mem__rdy,
	send_to_ctrl_mem__val,
	to_mem_raddr__msg,
	to_mem_raddr__rdy,
	to_mem_raddr__val,
	to_mem_waddr__msg,
	to_mem_waddr__rdy,
	to_mem_waddr__val,
	to_mem_wdata__msg,
	to_mem_wdata__rdy,
	to_mem_wdata__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [5:0] ctrl_addr_inport;
	input wire [0:0] reset;
	input wire [34:0] from_mem_rdata__msg;
	output reg [0:0] from_mem_rdata__rdy;
	input wire [0:0] from_mem_rdata__val;
	input wire [34:0] recv_const__msg;
	output reg [0:0] recv_const__rdy;
	input wire [0:0] recv_const__val;
	input wire [172:0] recv_from_ctrl_mem__msg;
	output reg [0:0] recv_from_ctrl_mem__rdy;
	input wire [0:0] recv_from_ctrl_mem__val;
	input wire [139:0] recv_in__msg;
	output reg [3:0] recv_in__rdy;
	input wire [3:0] recv_in__val;
	input wire [110:0] recv_opt__msg;
	output reg [0:0] recv_opt__rdy;
	input wire [0:0] recv_opt__val;
	output reg [69:0] send_out__msg;
	input wire [1:0] send_out__rdy;
	output reg [1:0] send_out__val;
	output reg [172:0] send_to_ctrl_mem__msg;
	input wire [0:0] send_to_ctrl_mem__rdy;
	output reg [0:0] send_to_ctrl_mem__val;
	output reg [14:0] to_mem_raddr__msg;
	input wire [0:0] to_mem_raddr__rdy;
	output reg [0:0] to_mem_raddr__val;
	output reg [14:0] to_mem_waddr__msg;
	input wire [0:0] to_mem_waddr__rdy;
	output reg [0:0] to_mem_waddr__val;
	output reg [34:0] to_mem_wdata__msg;
	input wire [0:0] to_mem_wdata__rdy;
	output reg [0:0] to_mem_wdata__val;
	localparam [34:0] const_zero = 35'h000000000;
	localparam [6:0] __const__OPT_START = 7'd0;
	localparam [0:0] __const__latency_at_proceed_latency = 1'd1;
	localparam [2:0] __const__num_inports_at_comb_logic = 3'd4;
	localparam [1:0] __const__num_outports_at_comb_logic = 2'd2;
	localparam [6:0] __const__OPT_NAH = 7'd1;
	reg [0:0] latency;
	reg [0:0] reached_vector_factor;
	reg [7:0] vector_factor_counter;
	wire [2:0] vector_factor_power;
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : comb_logic
		if (_sv2v_0)
			;
		recv_const__rdy = 1'd0;
		recv_opt__rdy = 1'd0;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_comb_logic; i = i + 1'd1)
				recv_in__rdy[3 - sv2v_cast_2(i)+:1] = 1'd0;
		end
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_comb_logic; i = i + 1'd1)
				begin
					send_out__val[1 - sv2v_cast_1(i)+:1] = 1'd0;
					send_out__msg[(1 - sv2v_cast_1(i)) * 35+:35] = 35'h000000000;
				end
		end
		send_to_ctrl_mem__val = 1'd0;
		send_to_ctrl_mem__msg = 173'h00000000000000000000000000000000000000000000;
		recv_from_ctrl_mem__rdy = 1'd0;
		if (recv_opt__val & (recv_opt__msg[110-:7] == __const__OPT_NAH))
			recv_opt__rdy = 1'd1;
		else begin
			begin : sv2v_autoblock_3
				reg [31:0] j;
				for (j = 1'd0; j < __const__num_outports_at_comb_logic; j = j + 1'd1)
					send_out__val[1 - sv2v_cast_1(j)+:1] = 1'd0;
			end
			recv_opt__rdy = 1'd0;
		end
	end
	always @(*) begin : update_mem
		if (_sv2v_0)
			;
		to_mem_waddr__val = 1'd0;
		to_mem_wdata__val = 1'd0;
		to_mem_wdata__msg = const_zero;
		to_mem_waddr__msg = 15'd0;
		to_mem_raddr__msg = 15'd0;
		to_mem_raddr__val = 1'd0;
		from_mem_rdata__rdy = 1'd0;
	end
	always @(*) begin : update_reached_vector_factor
		if (_sv2v_0)
			;
		reached_vector_factor = 1'd0;
		if (recv_opt__val & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) >= (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
			reached_vector_factor = 1'd1;
	end
	always @(posedge clk) begin : proceed_latency
		if ((reset | clear) | (recv_opt__msg[110-:7] == __const__OPT_START))
			latency <= 1'd0;
		else if (latency == (__const__latency_at_proceed_latency - 1'd1))
			latency <= 1'd0;
		else
			latency <= latency + 1'd1;
	end
	always @(posedge clk) begin : update_vector_factor_counter
		if (reset | clear)
			vector_factor_counter <= 8'd0;
		else if (recv_opt__val) begin
			if (recv_opt__msg[40] & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) < (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
				vector_factor_counter <= vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power});
			else if (recv_opt__msg[40] & reached_vector_factor)
				vector_factor_counter <= 8'd0;
		end
	end
	assign vector_factor_power = 3'd0;
	initial _sv2v_0 = 0;
endmodule
module MulRTL__00ec249fa9189fd7 (
	clear,
	clk,
	ctrl_addr_inport,
	reset,
	from_mem_rdata__msg,
	from_mem_rdata__rdy,
	from_mem_rdata__val,
	recv_const__msg,
	recv_const__rdy,
	recv_const__val,
	recv_from_ctrl_mem__msg,
	recv_from_ctrl_mem__rdy,
	recv_from_ctrl_mem__val,
	recv_in__msg,
	recv_in__rdy,
	recv_in__val,
	recv_opt__msg,
	recv_opt__rdy,
	recv_opt__val,
	send_out__msg,
	send_out__rdy,
	send_out__val,
	send_to_ctrl_mem__msg,
	send_to_ctrl_mem__rdy,
	send_to_ctrl_mem__val,
	to_mem_raddr__msg,
	to_mem_raddr__rdy,
	to_mem_raddr__val,
	to_mem_waddr__msg,
	to_mem_waddr__rdy,
	to_mem_waddr__val,
	to_mem_wdata__msg,
	to_mem_wdata__rdy,
	to_mem_wdata__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [5:0] ctrl_addr_inport;
	input wire [0:0] reset;
	input wire [34:0] from_mem_rdata__msg;
	output reg [0:0] from_mem_rdata__rdy;
	input wire [0:0] from_mem_rdata__val;
	input wire [34:0] recv_const__msg;
	output reg [0:0] recv_const__rdy;
	input wire [0:0] recv_const__val;
	input wire [172:0] recv_from_ctrl_mem__msg;
	output reg [0:0] recv_from_ctrl_mem__rdy;
	input wire [0:0] recv_from_ctrl_mem__val;
	input wire [139:0] recv_in__msg;
	output reg [3:0] recv_in__rdy;
	input wire [3:0] recv_in__val;
	input wire [110:0] recv_opt__msg;
	output reg [0:0] recv_opt__rdy;
	input wire [0:0] recv_opt__val;
	output reg [69:0] send_out__msg;
	input wire [1:0] send_out__rdy;
	output reg [1:0] send_out__val;
	output reg [172:0] send_to_ctrl_mem__msg;
	input wire [0:0] send_to_ctrl_mem__rdy;
	output reg [0:0] send_to_ctrl_mem__val;
	output reg [14:0] to_mem_raddr__msg;
	input wire [0:0] to_mem_raddr__rdy;
	output reg [0:0] to_mem_raddr__val;
	output reg [14:0] to_mem_waddr__msg;
	input wire [0:0] to_mem_waddr__rdy;
	output reg [0:0] to_mem_waddr__val;
	output reg [34:0] to_mem_wdata__msg;
	input wire [0:0] to_mem_wdata__rdy;
	output reg [0:0] to_mem_wdata__val;
	localparam [34:0] const_zero = 35'h000000000;
	localparam [6:0] __const__OPT_START = 7'd0;
	localparam [0:0] __const__latency_at_proceed_latency = 1'd1;
	localparam [2:0] __const__num_inports_at_comb_logic = 3'd4;
	localparam [1:0] __const__num_outports_at_comb_logic = 2'd2;
	localparam [6:0] __const__OPT_MUL = 7'd7;
	localparam [6:0] __const__OPT_MUL_CONST = 7'd29;
	reg [2:0] in0;
	wire [1:0] in0_idx;
	reg [2:0] in1;
	wire [1:0] in1_idx;
	reg [0:0] latency;
	reg [0:0] reached_vector_factor;
	reg [0:0] recv_all_val;
	reg [7:0] vector_factor_counter;
	wire [2:0] vector_factor_power;
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : comb_logic
		if (_sv2v_0)
			;
		recv_all_val = 1'd0;
		in0 = 3'd0;
		in1 = 3'd0;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_comb_logic; i = i + 1'd1)
				recv_in__rdy[3 - sv2v_cast_2(i)+:1] = 1'd0;
		end
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_comb_logic; i = i + 1'd1)
				begin
					send_out__val[1 - sv2v_cast_1(i)+:1] = 1'd0;
					send_out__msg[(1 - sv2v_cast_1(i)) * 35+:35] = 35'h000000000;
				end
		end
		recv_const__rdy = 1'd0;
		recv_opt__rdy = 1'd0;
		send_to_ctrl_mem__val = 1'd0;
		send_to_ctrl_mem__msg = 173'h00000000000000000000000000000000000000000000;
		recv_from_ctrl_mem__rdy = 1'd0;
		if (recv_opt__val) begin
			if (recv_opt__msg[92 + (2'd0 * 3)+:3] != 3'd0)
				in0 = recv_opt__msg[92 + (2'd0 * 3)+:3] - 3'd1;
			if (recv_opt__msg[92 + (2'd1 * 3)+:3] != 3'd0)
				in1 = recv_opt__msg[92 + (2'd1 * 3)+:3] - 3'd1;
		end
		if (recv_opt__val) begin
			if (recv_opt__msg[110-:7] == __const__OPT_MUL) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32] * recv_in__msg[((3 - in1_idx) * 35) + 34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_in__msg[((3 - in1_idx) * 35) + 2]) & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_in__val[3 - in1_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in1_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_MUL_CONST) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32] * recv_const__msg[34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = recv_in__msg[((3 - in0_idx) * 35) + 2] & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_const__val;
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_const__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else begin
				begin : sv2v_autoblock_3
					reg [31:0] j;
					for (j = 1'd0; j < __const__num_outports_at_comb_logic; j = j + 1'd1)
						send_out__val[1 - sv2v_cast_1(j)+:1] = 1'd0;
				end
				recv_opt__rdy = 1'd0;
				recv_in__rdy[3 - in0_idx+:1] = 1'd0;
				recv_in__rdy[3 - in1_idx+:1] = 1'd0;
			end
		end
	end
	always @(*) begin : update_mem
		if (_sv2v_0)
			;
		to_mem_waddr__val = 1'd0;
		to_mem_wdata__val = 1'd0;
		to_mem_wdata__msg = const_zero;
		to_mem_waddr__msg = 15'd0;
		to_mem_raddr__msg = 15'd0;
		to_mem_raddr__val = 1'd0;
		from_mem_rdata__rdy = 1'd0;
	end
	always @(*) begin : update_reached_vector_factor
		if (_sv2v_0)
			;
		reached_vector_factor = 1'd0;
		if (recv_opt__val & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) >= (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
			reached_vector_factor = 1'd1;
	end
	always @(posedge clk) begin : proceed_latency
		if ((reset | clear) | (recv_opt__msg[110-:7] == __const__OPT_START))
			latency <= 1'd0;
		else if (latency == (__const__latency_at_proceed_latency - 1'd1))
			latency <= 1'd0;
		else
			latency <= latency + 1'd1;
	end
	always @(posedge clk) begin : update_vector_factor_counter
		if (reset | clear)
			vector_factor_counter <= 8'd0;
		else if (recv_opt__val) begin
			if (recv_opt__msg[40] & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) < (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
				vector_factor_counter <= vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power});
			else if (recv_opt__msg[40] & reached_vector_factor)
				vector_factor_counter <= 8'd0;
		end
	end
	assign vector_factor_power = 3'd0;
	assign in0_idx = in0[1:0];
	assign in1_idx = in1[1:0];
	initial _sv2v_0 = 0;
endmodule
module ExclusiveDivRTL__00ec249fa9189fd7 (
	clear,
	clk,
	ctrl_addr_inport,
	reset,
	from_mem_rdata__msg,
	from_mem_rdata__rdy,
	from_mem_rdata__val,
	recv_const__msg,
	recv_const__rdy,
	recv_const__val,
	recv_from_ctrl_mem__msg,
	recv_from_ctrl_mem__rdy,
	recv_from_ctrl_mem__val,
	recv_in__msg,
	recv_in__rdy,
	recv_in__val,
	recv_opt__msg,
	recv_opt__rdy,
	recv_opt__val,
	send_out__msg,
	send_out__rdy,
	send_out__val,
	send_to_ctrl_mem__msg,
	send_to_ctrl_mem__rdy,
	send_to_ctrl_mem__val,
	to_mem_raddr__msg,
	to_mem_raddr__rdy,
	to_mem_raddr__val,
	to_mem_waddr__msg,
	to_mem_waddr__rdy,
	to_mem_waddr__val,
	to_mem_wdata__msg,
	to_mem_wdata__rdy,
	to_mem_wdata__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [5:0] ctrl_addr_inport;
	input wire [0:0] reset;
	input wire [34:0] from_mem_rdata__msg;
	output reg [0:0] from_mem_rdata__rdy;
	input wire [0:0] from_mem_rdata__val;
	input wire [34:0] recv_const__msg;
	output reg [0:0] recv_const__rdy;
	input wire [0:0] recv_const__val;
	input wire [172:0] recv_from_ctrl_mem__msg;
	output reg [0:0] recv_from_ctrl_mem__rdy;
	input wire [0:0] recv_from_ctrl_mem__val;
	input wire [139:0] recv_in__msg;
	output reg [3:0] recv_in__rdy;
	input wire [3:0] recv_in__val;
	input wire [110:0] recv_opt__msg;
	output reg [0:0] recv_opt__rdy;
	input wire [0:0] recv_opt__val;
	output reg [69:0] send_out__msg;
	input wire [1:0] send_out__rdy;
	output reg [1:0] send_out__val;
	output reg [172:0] send_to_ctrl_mem__msg;
	input wire [0:0] send_to_ctrl_mem__rdy;
	output reg [0:0] send_to_ctrl_mem__val;
	output reg [14:0] to_mem_raddr__msg;
	input wire [0:0] to_mem_raddr__rdy;
	output reg [0:0] to_mem_raddr__val;
	output reg [14:0] to_mem_waddr__msg;
	input wire [0:0] to_mem_waddr__rdy;
	output reg [0:0] to_mem_waddr__val;
	output reg [34:0] to_mem_wdata__msg;
	input wire [0:0] to_mem_wdata__rdy;
	output reg [0:0] to_mem_wdata__val;
	localparam [34:0] const_zero = 35'h000000000;
	localparam [6:0] __const__OPT_START = 7'd0;
	localparam [0:0] __const__latency_at_proceed_latency = 1'd1;
	localparam [2:0] __const__num_inports_at_ports = 3'd4;
	localparam [1:0] __const__num_outports_at_ports = 2'd2;
	localparam [6:0] __const__OPT_DIV_CONST = 7'd65;
	localparam [6:0] __const__OPT_SQRT = 7'd102;
	localparam [6:0] __const__OPT_DIV = 7'd26;
	localparam [6:0] __const__OPT_REM = 7'd44;
	localparam [5:0] __const__width_at_restoring_step = 6'd32;
	localparam [4:0] __const__digits_at_restoring_step = 5'd16;
	localparam [5:0] __const__width_at_state = 6'd32;
	localparam [4:0] __const__digits_at_state = 5'd16;
	reg [0:0] busy;
	reg [5:0] count;
	reg [31:0] divisor;
	reg [0:0] done;
	reg [1:0] in0;
	reg [1:0] in1;
	reg [31:0] input_divisor;
	reg [0:0] input_predicate;
	reg [0:0] is_sqrt;
	reg [0:0] latency;
	reg [0:0] predicate;
	reg [31:0] quotient;
	reg [0:0] reached_vector_factor;
	reg [32:0] remainder;
	reg [0:0] start;
	reg [32:0] subtract;
	reg [0:0] take_remainder;
	reg [32:0] trial;
	reg [7:0] vector_factor_counter;
	wire [2:0] vector_factor_power;
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	function automatic [31:0] sv2v_cast_32;
		input reg [31:0] inp;
		sv2v_cast_32 = inp;
	endfunction
	always @(*) begin : ports
		if (_sv2v_0)
			;
		in0 = 2'd0;
		in1 = 2'd0;
		if (recv_opt__msg[92 + (2'd0 * 3)+:3] != 3'd0)
			in0 = sv2v_cast_2(recv_opt__msg[92 + (2'd0 * 3)+:3] - 3'd1);
		if (recv_opt__msg[92 + (2'd1 * 3)+:3] != 3'd0)
			in1 = sv2v_cast_2(recv_opt__msg[92 + (2'd1 * 3)+:3] - 3'd1);
		start = 1'd0;
		recv_opt__rdy = 1'd0;
		recv_const__rdy = 1'd0;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_ports; i = i + 1'd1)
				recv_in__rdy[3 - sv2v_cast_2(i)+:1] = 1'd0;
		end
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_ports; i = i + 1'd1)
				begin
					send_out__val[1 - sv2v_cast_1(i)+:1] = 1'd0;
					send_out__msg[(1 - sv2v_cast_1(i)) * 35+:35] = 35'h000000000;
				end
		end
		send_to_ctrl_mem__val = 1'd0;
		send_to_ctrl_mem__msg = 173'h00000000000000000000000000000000000000000000;
		recv_from_ctrl_mem__rdy = 1'd0;
		input_divisor = recv_in__msg[((3 - in1) * 35) + 34-:32];
		input_predicate = recv_in__msg[((3 - in0) * 35) + 2] & recv_in__msg[((3 - in1) * 35) + 2];
		if (recv_opt__msg[110-:7] == __const__OPT_DIV_CONST) begin
			input_divisor = recv_const__msg[34-:32];
			input_predicate = recv_in__msg[((3 - in0) * 35) + 2];
		end
		if (recv_opt__msg[110-:7] == __const__OPT_SQRT)
			input_predicate = recv_in__msg[((3 - in0) * 35) + 2];
		if (recv_opt__val & ~clear) begin
			if ((((recv_opt__msg[110-:7] == __const__OPT_DIV) | (recv_opt__msg[110-:7] == __const__OPT_DIV_CONST)) | (recv_opt__msg[110-:7] == __const__OPT_REM)) | (recv_opt__msg[110-:7] == __const__OPT_SQRT)) begin
				if (~busy & ~done) begin
					if (recv_opt__msg[110-:7] == __const__OPT_DIV_CONST)
						start = recv_in__val[3 - in0+:1] & recv_const__val;
					else if (recv_opt__msg[110-:7] == __const__OPT_SQRT)
						start = recv_in__val[3 - in0+:1];
					else
						start = recv_in__val[3 - in0+:1] & recv_in__val[3 - in1+:1];
				end
				send_out__val[(1 - 1'd0) + 0+:1] = done;
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = quotient;
				if (take_remainder)
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = sv2v_cast_32(remainder);
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = predicate;
				recv_opt__rdy = done & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in0+:1] = recv_opt__rdy;
				if (recv_opt__msg[110-:7] == __const__OPT_DIV_CONST)
					recv_const__rdy = recv_opt__rdy;
				else if (recv_opt__msg[110-:7] != __const__OPT_SQRT)
					recv_in__rdy[3 - in1+:1] = recv_opt__rdy;
			end
		end
	end
	always @(*) begin : restoring_step
		if (_sv2v_0)
			;
		trial = (remainder << 1'd1) | {{32 {1'b0}}, quotient[5'd31:__const__width_at_restoring_step - 6'd1]};
		subtract = {1'b0, divisor};
		if (is_sqrt) begin
			trial = (remainder << 2'd2) | {{31 {1'b0}}, divisor[6'd31:(__const__digits_at_restoring_step * 5'd2) - 6'd2]};
			subtract = ({1'b0, quotient} << 2'd2) | 33'd1;
		end
	end
	always @(*) begin : update_mem
		if (_sv2v_0)
			;
		to_mem_waddr__val = 1'd0;
		to_mem_wdata__val = 1'd0;
		to_mem_wdata__msg = const_zero;
		to_mem_waddr__msg = 15'd0;
		to_mem_raddr__msg = 15'd0;
		to_mem_raddr__val = 1'd0;
		from_mem_rdata__rdy = 1'd0;
	end
	always @(*) begin : update_reached_vector_factor
		if (_sv2v_0)
			;
		reached_vector_factor = 1'd0;
		if (recv_opt__val & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) >= (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
			reached_vector_factor = 1'd1;
	end
	always @(posedge clk) begin : proceed_latency
		if ((reset | clear) | (recv_opt__msg[110-:7] == __const__OPT_START))
			latency <= 1'd0;
		else if (latency == (__const__latency_at_proceed_latency - 1'd1))
			latency <= 1'd0;
		else
			latency <= latency + 1'd1;
	end
	function automatic [5:0] sv2v_cast_6;
		input reg [5:0] inp;
		sv2v_cast_6 = inp;
	endfunction
	always @(posedge clk) begin : state
		if (reset | clear) begin
			busy <= 1'd0;
			done <= 1'd0;
			count <= 6'd0;
			quotient <= 32'd0;
			divisor <= 32'd0;
			remainder <= 33'd0;
			predicate <= 1'd0;
			take_remainder <= 1'd0;
			is_sqrt <= 1'd0;
		end
		else if (start) begin
			busy <= 1'd1;
			count <= __const__width_at_state;
			quotient <= recv_in__msg[((3 - in0) * 35) + 34-:32];
			divisor <= input_divisor;
			is_sqrt <= recv_opt__msg[110-:7] == __const__OPT_SQRT;
			if (recv_opt__msg[110-:7] == __const__OPT_SQRT) begin
				count <= sv2v_cast_6(__const__digits_at_state);
				quotient <= 32'd0;
				divisor <= recv_in__msg[((3 - in0) * 35) + 34-:32];
			end
			remainder <= 33'd0;
			predicate <= input_predicate & reached_vector_factor;
			take_remainder <= recv_opt__msg[110-:7] == __const__OPT_REM;
		end
		else if (busy) begin
			count <= count - 6'd1;
			if (is_sqrt)
				divisor <= divisor << 2'd2;
			if (trial >= subtract) begin
				remainder <= trial - subtract;
				quotient <= (quotient << 1'd1) | 32'd1;
			end
			else begin
				remainder <= trial;
				quotient <= quotient << 1'd1;
			end
			if (count == 6'd1) begin
				busy <= 1'd0;
				done <= 1'd1;
			end
		end
		else if (recv_opt__val & recv_opt__rdy)
			done <= 1'd0;
	end
	always @(posedge clk) begin : update_vector_factor_counter
		if (reset | clear)
			vector_factor_counter <= 8'd0;
		else if (recv_opt__val) begin
			if (recv_opt__msg[40] & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) < (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
				vector_factor_counter <= vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power});
			else if (recv_opt__msg[40] & reached_vector_factor)
				vector_factor_counter <= 8'd0;
		end
	end
	assign vector_factor_power = 3'd0;
	initial _sv2v_0 = 0;
endmodule
module LogicRTL__00ec249fa9189fd7 (
	clear,
	clk,
	ctrl_addr_inport,
	reset,
	from_mem_rdata__msg,
	from_mem_rdata__rdy,
	from_mem_rdata__val,
	recv_const__msg,
	recv_const__rdy,
	recv_const__val,
	recv_from_ctrl_mem__msg,
	recv_from_ctrl_mem__rdy,
	recv_from_ctrl_mem__val,
	recv_in__msg,
	recv_in__rdy,
	recv_in__val,
	recv_opt__msg,
	recv_opt__rdy,
	recv_opt__val,
	send_out__msg,
	send_out__rdy,
	send_out__val,
	send_to_ctrl_mem__msg,
	send_to_ctrl_mem__rdy,
	send_to_ctrl_mem__val,
	to_mem_raddr__msg,
	to_mem_raddr__rdy,
	to_mem_raddr__val,
	to_mem_waddr__msg,
	to_mem_waddr__rdy,
	to_mem_waddr__val,
	to_mem_wdata__msg,
	to_mem_wdata__rdy,
	to_mem_wdata__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [5:0] ctrl_addr_inport;
	input wire [0:0] reset;
	input wire [34:0] from_mem_rdata__msg;
	output reg [0:0] from_mem_rdata__rdy;
	input wire [0:0] from_mem_rdata__val;
	input wire [34:0] recv_const__msg;
	output reg [0:0] recv_const__rdy;
	input wire [0:0] recv_const__val;
	input wire [172:0] recv_from_ctrl_mem__msg;
	output reg [0:0] recv_from_ctrl_mem__rdy;
	input wire [0:0] recv_from_ctrl_mem__val;
	input wire [139:0] recv_in__msg;
	output reg [3:0] recv_in__rdy;
	input wire [3:0] recv_in__val;
	input wire [110:0] recv_opt__msg;
	output reg [0:0] recv_opt__rdy;
	input wire [0:0] recv_opt__val;
	output reg [69:0] send_out__msg;
	input wire [1:0] send_out__rdy;
	output reg [1:0] send_out__val;
	output reg [172:0] send_to_ctrl_mem__msg;
	input wire [0:0] send_to_ctrl_mem__rdy;
	output reg [0:0] send_to_ctrl_mem__val;
	output reg [14:0] to_mem_raddr__msg;
	input wire [0:0] to_mem_raddr__rdy;
	output reg [0:0] to_mem_raddr__val;
	output reg [14:0] to_mem_waddr__msg;
	input wire [0:0] to_mem_waddr__rdy;
	output reg [0:0] to_mem_waddr__val;
	output reg [34:0] to_mem_wdata__msg;
	input wire [0:0] to_mem_wdata__rdy;
	output reg [0:0] to_mem_wdata__val;
	localparam [34:0] const_zero = 35'h000000000;
	localparam [6:0] __const__OPT_START = 7'd0;
	localparam [0:0] __const__latency_at_proceed_latency = 1'd1;
	localparam [2:0] __const__num_inports_at_comb_logic = 3'd4;
	localparam [1:0] __const__num_outports_at_comb_logic = 2'd2;
	localparam [6:0] __const__OPT_OR = 7'd8;
	localparam [6:0] __const__OPT_AND = 7'd10;
	localparam [6:0] __const__OPT_BIT_NOT = 7'd43;
	localparam [6:0] __const__OPT_NOT = 7'd11;
	localparam [6:0] __const__OPT_XOR = 7'd9;
	reg [2:0] in0;
	wire [1:0] in0_idx;
	reg [2:0] in1;
	wire [1:0] in1_idx;
	reg [0:0] latency;
	reg [0:0] reached_vector_factor;
	reg [0:0] recv_all_val;
	reg [7:0] vector_factor_counter;
	wire [2:0] vector_factor_power;
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : comb_logic
		if (_sv2v_0)
			;
		recv_all_val = 1'd0;
		in0 = 3'd0;
		in1 = 3'd0;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_comb_logic; i = i + 1'd1)
				recv_in__rdy[3 - sv2v_cast_2(i)+:1] = 1'd0;
		end
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_comb_logic; i = i + 1'd1)
				begin
					send_out__val[1 - sv2v_cast_1(i)+:1] = 1'd0;
					send_out__msg[(1 - sv2v_cast_1(i)) * 35+:35] = 35'h000000000;
				end
		end
		recv_const__rdy = 1'd0;
		recv_opt__rdy = 1'd0;
		send_to_ctrl_mem__val = 1'd0;
		send_to_ctrl_mem__msg = 173'h00000000000000000000000000000000000000000000;
		recv_from_ctrl_mem__rdy = 1'd0;
		if (recv_opt__val) begin
			if (recv_opt__msg[92 + (2'd0 * 3)+:3] != 3'd0)
				in0 = recv_opt__msg[92 + (2'd0 * 3)+:3] - 3'd1;
			if (recv_opt__msg[92 + (2'd1 * 3)+:3] != 3'd0)
				in1 = recv_opt__msg[92 + (2'd1 * 3)+:3] - 3'd1;
		end
		if (recv_opt__val) begin
			if (recv_opt__msg[110-:7] == __const__OPT_OR) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32] | recv_in__msg[((3 - in1_idx) * 35) + 34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_in__msg[((3 - in1_idx) * 35) + 2]) & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_in__val[3 - in1_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in1_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_AND) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32] & recv_in__msg[((3 - in1_idx) * 35) + 34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_in__msg[((3 - in1_idx) * 35) + 2]) & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_in__val[3 - in1_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in1_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_BIT_NOT) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = ~recv_in__msg[((3 - in0_idx) * 35) + 34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = recv_in__msg[((3 - in0_idx) * 35) + 2] & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_NOT) begin
				if (recv_in__msg[((3 - in0_idx) * 35) + 34-:32] == 32'd0)
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = 32'd1;
				else
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = 32'd0;
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = recv_in__msg[((3 - in0_idx) * 35) + 2] & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_XOR) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32] ^ recv_in__msg[((3 - in1_idx) * 35) + 34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_in__msg[((3 - in1_idx) * 35) + 2]) & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_in__val[3 - in1_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in1_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else begin
				begin : sv2v_autoblock_3
					reg [31:0] j;
					for (j = 1'd0; j < __const__num_outports_at_comb_logic; j = j + 1'd1)
						send_out__val[1 - sv2v_cast_1(j)+:1] = 1'd0;
				end
				recv_opt__rdy = 1'd0;
				recv_in__rdy[3 - in0_idx+:1] = 1'd0;
				recv_in__rdy[3 - in1_idx+:1] = 1'd0;
			end
		end
	end
	always @(*) begin : update_mem
		if (_sv2v_0)
			;
		to_mem_waddr__val = 1'd0;
		to_mem_wdata__val = 1'd0;
		to_mem_wdata__msg = const_zero;
		to_mem_waddr__msg = 15'd0;
		to_mem_raddr__msg = 15'd0;
		to_mem_raddr__val = 1'd0;
		from_mem_rdata__rdy = 1'd0;
	end
	always @(*) begin : update_reached_vector_factor
		if (_sv2v_0)
			;
		reached_vector_factor = 1'd0;
		if (recv_opt__val & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) >= (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
			reached_vector_factor = 1'd1;
	end
	always @(posedge clk) begin : proceed_latency
		if ((reset | clear) | (recv_opt__msg[110-:7] == __const__OPT_START))
			latency <= 1'd0;
		else if (latency == (__const__latency_at_proceed_latency - 1'd1))
			latency <= 1'd0;
		else
			latency <= latency + 1'd1;
	end
	always @(posedge clk) begin : update_vector_factor_counter
		if (reset | clear)
			vector_factor_counter <= 8'd0;
		else if (recv_opt__val) begin
			if (recv_opt__msg[40] & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) < (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
				vector_factor_counter <= vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power});
			else if (recv_opt__msg[40] & reached_vector_factor)
				vector_factor_counter <= 8'd0;
		end
	end
	assign vector_factor_power = 3'd0;
	assign in0_idx = in0[1:0];
	assign in1_idx = in1[1:0];
	initial _sv2v_0 = 0;
endmodule
module ShifterRTL__00ec249fa9189fd7 (
	clear,
	clk,
	ctrl_addr_inport,
	reset,
	from_mem_rdata__msg,
	from_mem_rdata__rdy,
	from_mem_rdata__val,
	recv_const__msg,
	recv_const__rdy,
	recv_const__val,
	recv_from_ctrl_mem__msg,
	recv_from_ctrl_mem__rdy,
	recv_from_ctrl_mem__val,
	recv_in__msg,
	recv_in__rdy,
	recv_in__val,
	recv_opt__msg,
	recv_opt__rdy,
	recv_opt__val,
	send_out__msg,
	send_out__rdy,
	send_out__val,
	send_to_ctrl_mem__msg,
	send_to_ctrl_mem__rdy,
	send_to_ctrl_mem__val,
	to_mem_raddr__msg,
	to_mem_raddr__rdy,
	to_mem_raddr__val,
	to_mem_waddr__msg,
	to_mem_waddr__rdy,
	to_mem_waddr__val,
	to_mem_wdata__msg,
	to_mem_wdata__rdy,
	to_mem_wdata__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [5:0] ctrl_addr_inport;
	input wire [0:0] reset;
	input wire [34:0] from_mem_rdata__msg;
	output reg [0:0] from_mem_rdata__rdy;
	input wire [0:0] from_mem_rdata__val;
	input wire [34:0] recv_const__msg;
	output reg [0:0] recv_const__rdy;
	input wire [0:0] recv_const__val;
	input wire [172:0] recv_from_ctrl_mem__msg;
	output reg [0:0] recv_from_ctrl_mem__rdy;
	input wire [0:0] recv_from_ctrl_mem__val;
	input wire [139:0] recv_in__msg;
	output reg [3:0] recv_in__rdy;
	input wire [3:0] recv_in__val;
	input wire [110:0] recv_opt__msg;
	output reg [0:0] recv_opt__rdy;
	input wire [0:0] recv_opt__val;
	output reg [69:0] send_out__msg;
	input wire [1:0] send_out__rdy;
	output reg [1:0] send_out__val;
	output reg [172:0] send_to_ctrl_mem__msg;
	input wire [0:0] send_to_ctrl_mem__rdy;
	output reg [0:0] send_to_ctrl_mem__val;
	output reg [14:0] to_mem_raddr__msg;
	input wire [0:0] to_mem_raddr__rdy;
	output reg [0:0] to_mem_raddr__val;
	output reg [14:0] to_mem_waddr__msg;
	input wire [0:0] to_mem_waddr__rdy;
	output reg [0:0] to_mem_waddr__val;
	output reg [34:0] to_mem_wdata__msg;
	input wire [0:0] to_mem_wdata__rdy;
	output reg [0:0] to_mem_wdata__val;
	localparam [34:0] const_zero = 35'h000000000;
	localparam [6:0] __const__OPT_START = 7'd0;
	localparam [0:0] __const__latency_at_proceed_latency = 1'd1;
	localparam [2:0] __const__num_inports_at_comb_logic = 3'd4;
	localparam [1:0] __const__num_outports_at_comb_logic = 2'd2;
	localparam [6:0] __const__OPT_LLS = 7'd5;
	localparam [6:0] __const__OPT_LLS_CONST = 7'd96;
	localparam [6:0] __const__OPT_LRS = 7'd6;
	reg [2:0] in0;
	wire [1:0] in0_idx;
	reg [2:0] in1;
	wire [1:0] in1_idx;
	reg [0:0] latency;
	reg [0:0] reached_vector_factor;
	reg [0:0] recv_all_val;
	reg [7:0] vector_factor_counter;
	wire [2:0] vector_factor_power;
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : comb_logic
		if (_sv2v_0)
			;
		recv_all_val = 1'd0;
		in0 = 3'd0;
		in1 = 3'd0;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_comb_logic; i = i + 1'd1)
				recv_in__rdy[3 - sv2v_cast_2(i)+:1] = 1'd0;
		end
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_comb_logic; i = i + 1'd1)
				begin
					send_out__val[1 - sv2v_cast_1(i)+:1] = 1'd0;
					send_out__msg[(1 - sv2v_cast_1(i)) * 35+:35] = 35'h000000000;
				end
		end
		recv_const__rdy = 1'd0;
		recv_opt__rdy = 1'd0;
		send_to_ctrl_mem__val = 1'd0;
		send_to_ctrl_mem__msg = 173'h00000000000000000000000000000000000000000000;
		recv_from_ctrl_mem__rdy = 1'd0;
		if (recv_opt__val) begin
			if (recv_opt__msg[92 + (2'd0 * 3)+:3] != 3'd0)
				in0 = recv_opt__msg[92 + (2'd0 * 3)+:3] - 3'd1;
			if (recv_opt__msg[92 + (2'd1 * 3)+:3] != 3'd0)
				in1 = recv_opt__msg[92 + (2'd1 * 3)+:3] - 3'd1;
		end
		if (recv_opt__val) begin
			if (recv_opt__msg[110-:7] == __const__OPT_LLS) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32] << recv_in__msg[((3 - in1_idx) * 35) + 34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_in__msg[((3 - in1_idx) * 35) + 2]) & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_in__val[3 - in1_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in1_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_LLS_CONST) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32] << recv_const__msg[34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_const__msg[2]) & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_const__val;
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_const__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_LRS) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32] >> recv_in__msg[((3 - in1_idx) * 35) + 34-:32];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_in__msg[((3 - in1_idx) * 35) + 2]) & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_in__val[3 - in1_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in1_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else begin
				begin : sv2v_autoblock_3
					reg [31:0] j;
					for (j = 1'd0; j < __const__num_outports_at_comb_logic; j = j + 1'd1)
						send_out__val[1 - sv2v_cast_1(j)+:1] = 1'd0;
				end
				recv_opt__rdy = 1'd0;
				recv_in__rdy[3 - in0_idx+:1] = 1'd0;
				recv_in__rdy[3 - in1_idx+:1] = 1'd0;
			end
		end
	end
	always @(*) begin : update_mem
		if (_sv2v_0)
			;
		to_mem_waddr__val = 1'd0;
		to_mem_wdata__val = 1'd0;
		to_mem_wdata__msg = const_zero;
		to_mem_waddr__msg = 15'd0;
		to_mem_raddr__msg = 15'd0;
		to_mem_raddr__val = 1'd0;
		from_mem_rdata__rdy = 1'd0;
	end
	always @(*) begin : update_reached_vector_factor
		if (_sv2v_0)
			;
		reached_vector_factor = 1'd0;
		if (recv_opt__val & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) >= (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
			reached_vector_factor = 1'd1;
	end
	always @(posedge clk) begin : proceed_latency
		if ((reset | clear) | (recv_opt__msg[110-:7] == __const__OPT_START))
			latency <= 1'd0;
		else if (latency == (__const__latency_at_proceed_latency - 1'd1))
			latency <= 1'd0;
		else
			latency <= latency + 1'd1;
	end
	always @(posedge clk) begin : update_vector_factor_counter
		if (reset | clear)
			vector_factor_counter <= 8'd0;
		else if (recv_opt__val) begin
			if (recv_opt__msg[40] & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) < (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
				vector_factor_counter <= vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power});
			else if (recv_opt__msg[40] & reached_vector_factor)
				vector_factor_counter <= 8'd0;
		end
	end
	assign vector_factor_power = 3'd0;
	assign in0_idx = in0[1:0];
	assign in1_idx = in1[1:0];
	initial _sv2v_0 = 0;
endmodule
module CompRTL__00ec249fa9189fd7 (
	clear,
	clk,
	ctrl_addr_inport,
	reset,
	from_mem_rdata__msg,
	from_mem_rdata__rdy,
	from_mem_rdata__val,
	recv_const__msg,
	recv_const__rdy,
	recv_const__val,
	recv_from_ctrl_mem__msg,
	recv_from_ctrl_mem__rdy,
	recv_from_ctrl_mem__val,
	recv_in__msg,
	recv_in__rdy,
	recv_in__val,
	recv_opt__msg,
	recv_opt__rdy,
	recv_opt__val,
	send_out__msg,
	send_out__rdy,
	send_out__val,
	send_to_ctrl_mem__msg,
	send_to_ctrl_mem__rdy,
	send_to_ctrl_mem__val,
	to_mem_raddr__msg,
	to_mem_raddr__rdy,
	to_mem_raddr__val,
	to_mem_waddr__msg,
	to_mem_waddr__rdy,
	to_mem_waddr__val,
	to_mem_wdata__msg,
	to_mem_wdata__rdy,
	to_mem_wdata__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [5:0] ctrl_addr_inport;
	input wire [0:0] reset;
	input wire [34:0] from_mem_rdata__msg;
	output reg [0:0] from_mem_rdata__rdy;
	input wire [0:0] from_mem_rdata__val;
	input wire [34:0] recv_const__msg;
	output reg [0:0] recv_const__rdy;
	input wire [0:0] recv_const__val;
	input wire [172:0] recv_from_ctrl_mem__msg;
	output reg [0:0] recv_from_ctrl_mem__rdy;
	input wire [0:0] recv_from_ctrl_mem__val;
	input wire [139:0] recv_in__msg;
	output reg [3:0] recv_in__rdy;
	input wire [3:0] recv_in__val;
	input wire [110:0] recv_opt__msg;
	output reg [0:0] recv_opt__rdy;
	input wire [0:0] recv_opt__val;
	output reg [69:0] send_out__msg;
	input wire [1:0] send_out__rdy;
	output reg [1:0] send_out__val;
	output reg [172:0] send_to_ctrl_mem__msg;
	input wire [0:0] send_to_ctrl_mem__rdy;
	output reg [0:0] send_to_ctrl_mem__val;
	output reg [14:0] to_mem_raddr__msg;
	input wire [0:0] to_mem_raddr__rdy;
	output reg [0:0] to_mem_raddr__val;
	output reg [14:0] to_mem_waddr__msg;
	input wire [0:0] to_mem_waddr__rdy;
	output reg [0:0] to_mem_waddr__val;
	output reg [34:0] to_mem_wdata__msg;
	input wire [0:0] to_mem_wdata__rdy;
	output reg [0:0] to_mem_wdata__val;
	localparam [34:0] const_one = 35'h000000008;
	localparam [34:0] const_zero = 35'h000000000;
	localparam [6:0] __const__OPT_START = 7'd0;
	localparam [0:0] __const__latency_at_proceed_latency = 1'd1;
	localparam [2:0] __const__num_inports_at_read_reg = 3'd4;
	localparam [1:0] __const__num_outports_at_read_reg = 2'd2;
	localparam [6:0] __const__OPT_EQ = 7'd14;
	localparam [6:0] __const__OPT_NE = 7'd45;
	localparam [6:0] __const__OPT_EQ_CONST = 7'd33;
	localparam [6:0] __const__OPT_NE_CONST = 7'd46;
	localparam [6:0] __const__OPT_LT = 7'd60;
	localparam [31:0] __const__sign_flip_at_read_reg = 32'd2147483648;
	localparam [6:0] __const__OPT_GTE_CONST = 7'd90;
	localparam [6:0] __const__OPT_GT_CONST = 7'd97;
	localparam [6:0] __const__OPT_LT_CONST = 7'd98;
	reg [2:0] in0;
	wire [1:0] in0_idx;
	reg [2:0] in1;
	wire [1:0] in1_idx;
	reg [0:0] latency;
	reg [0:0] reached_vector_factor;
	reg [0:0] recv_all_val;
	reg [7:0] vector_factor_counter;
	wire [2:0] vector_factor_power;
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : read_reg
		if (_sv2v_0)
			;
		recv_all_val = 1'd0;
		in0 = 3'd0;
		in1 = 3'd0;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_read_reg; i = i + 1'd1)
				recv_in__rdy[3 - sv2v_cast_2(i)+:1] = 1'd0;
		end
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_read_reg; i = i + 1'd1)
				begin
					send_out__val[1 - sv2v_cast_1(i)+:1] = 1'd0;
					send_out__msg[(1 - sv2v_cast_1(i)) * 35+:35] = 35'h000000000;
				end
		end
		recv_const__rdy = 1'd0;
		recv_opt__rdy = 1'd0;
		send_to_ctrl_mem__val = 1'd0;
		send_to_ctrl_mem__msg = 173'h00000000000000000000000000000000000000000000;
		recv_from_ctrl_mem__rdy = 1'd0;
		if (recv_opt__val) begin
			if (recv_opt__msg[92 + (2'd0 * 3)+:3] != 3'd0)
				in0 = recv_opt__msg[92 + (2'd0 * 3)+:3] - 3'd1;
			if (recv_opt__msg[92 + (2'd1 * 3)+:3] != 3'd0)
				in1 = recv_opt__msg[92 + (2'd1 * 3)+:3] - 3'd1;
		end
		if (recv_opt__val) begin
			if ((recv_opt__msg[110-:7] == __const__OPT_EQ) | (recv_opt__msg[110-:7] == __const__OPT_NE)) begin
				if ((recv_opt__msg[110-:7] == __const__OPT_EQ) & (recv_in__msg[((3 - in0_idx) * 35) + 34-:32] == recv_in__msg[((3 - in1_idx) * 35) + 34-:32]))
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = const_one;
				else if ((recv_opt__msg[110-:7] == __const__OPT_NE) & (recv_in__msg[((3 - in0_idx) * 35) + 34-:32] != recv_in__msg[((3 - in1_idx) * 35) + 34-:32]))
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = const_one;
				else
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = const_zero;
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_in__msg[((3 - in1_idx) * 35) + 2]) & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_in__val[3 - in1_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in1_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if ((recv_opt__msg[110-:7] == __const__OPT_EQ_CONST) | (recv_opt__msg[110-:7] == __const__OPT_NE_CONST)) begin
				if ((recv_opt__msg[110-:7] == __const__OPT_EQ_CONST) & (recv_in__msg[((3 - in0_idx) * 35) + 34-:32] == recv_const__msg[34-:32]))
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = const_one;
				else if ((recv_opt__msg[110-:7] == __const__OPT_NE_CONST) & (recv_in__msg[((3 - in0_idx) * 35) + 34-:32] != recv_const__msg[34-:32]))
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = const_one;
				else
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = const_zero;
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_const__msg[2]) & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_const__val;
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_const__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_LT) begin
				if ((recv_in__msg[((3 - in0_idx) * 35) + 34-:32] ^ __const__sign_flip_at_read_reg) < (recv_in__msg[((3 - in1_idx) * 35) + 34-:32] ^ __const__sign_flip_at_read_reg))
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = const_one;
				else
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = const_zero;
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_in__msg[((3 - in1_idx) * 35) + 2]) & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_in__val[3 - in1_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in1_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_GTE_CONST) begin
				if ((recv_in__msg[((3 - in0_idx) * 35) + 34-:32] ^ __const__sign_flip_at_read_reg) >= (recv_const__msg[34-:32] ^ __const__sign_flip_at_read_reg))
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = const_one;
				else
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = const_zero;
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_const__msg[2]) & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_const__val;
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_const__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_GT_CONST) begin
				if ((recv_in__msg[((3 - in0_idx) * 35) + 34-:32] ^ __const__sign_flip_at_read_reg) > (recv_const__msg[34-:32] ^ __const__sign_flip_at_read_reg))
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = const_one;
				else
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = const_zero;
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_const__msg[2]) & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_const__val;
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_const__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_LT_CONST) begin
				if ((recv_in__msg[((3 - in0_idx) * 35) + 34-:32] ^ __const__sign_flip_at_read_reg) < (recv_const__msg[34-:32] ^ __const__sign_flip_at_read_reg))
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = const_one;
				else
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = const_zero;
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_const__msg[2]) & reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_const__val;
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_const__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else begin
				begin : sv2v_autoblock_3
					reg [31:0] j;
					for (j = 1'd0; j < __const__num_outports_at_read_reg; j = j + 1'd1)
						send_out__val[1 - sv2v_cast_1(j)+:1] = 1'd0;
				end
				recv_opt__rdy = 1'd0;
				recv_in__rdy[3 - in0_idx+:1] = 1'd0;
				recv_in__rdy[3 - in1_idx+:1] = 1'd0;
			end
		end
	end
	always @(*) begin : update_mem
		if (_sv2v_0)
			;
		to_mem_waddr__val = 1'd0;
		to_mem_wdata__val = 1'd0;
		to_mem_wdata__msg = const_zero;
		to_mem_waddr__msg = 15'd0;
		to_mem_raddr__msg = 15'd0;
		to_mem_raddr__val = 1'd0;
		from_mem_rdata__rdy = 1'd0;
	end
	always @(*) begin : update_reached_vector_factor
		if (_sv2v_0)
			;
		reached_vector_factor = 1'd0;
		if (recv_opt__val & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) >= (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
			reached_vector_factor = 1'd1;
	end
	always @(posedge clk) begin : proceed_latency
		if ((reset | clear) | (recv_opt__msg[110-:7] == __const__OPT_START))
			latency <= 1'd0;
		else if (latency == (__const__latency_at_proceed_latency - 1'd1))
			latency <= 1'd0;
		else
			latency <= latency + 1'd1;
	end
	always @(posedge clk) begin : update_vector_factor_counter
		if (reset | clear)
			vector_factor_counter <= 8'd0;
		else if (recv_opt__val) begin
			if (recv_opt__msg[40] & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) < (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
				vector_factor_counter <= vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power});
			else if (recv_opt__msg[40] & reached_vector_factor)
				vector_factor_counter <= 8'd0;
		end
	end
	assign vector_factor_power = 3'd0;
	assign in0_idx = in0[1:0];
	assign in1_idx = in1[1:0];
	initial _sv2v_0 = 0;
endmodule
module SelRTL__00ec249fa9189fd7 (
	clear,
	clk,
	ctrl_addr_inport,
	reset,
	from_mem_rdata__msg,
	from_mem_rdata__rdy,
	from_mem_rdata__val,
	recv_const__msg,
	recv_const__rdy,
	recv_const__val,
	recv_from_ctrl_mem__msg,
	recv_from_ctrl_mem__rdy,
	recv_from_ctrl_mem__val,
	recv_in__msg,
	recv_in__rdy,
	recv_in__val,
	recv_opt__msg,
	recv_opt__rdy,
	recv_opt__val,
	send_out__msg,
	send_out__rdy,
	send_out__val,
	send_to_ctrl_mem__msg,
	send_to_ctrl_mem__rdy,
	send_to_ctrl_mem__val,
	to_mem_raddr__msg,
	to_mem_raddr__rdy,
	to_mem_raddr__val,
	to_mem_waddr__msg,
	to_mem_waddr__rdy,
	to_mem_waddr__val,
	to_mem_wdata__msg,
	to_mem_wdata__rdy,
	to_mem_wdata__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [5:0] ctrl_addr_inport;
	input wire [0:0] reset;
	input wire [34:0] from_mem_rdata__msg;
	output reg [0:0] from_mem_rdata__rdy;
	input wire [0:0] from_mem_rdata__val;
	input wire [34:0] recv_const__msg;
	output reg [0:0] recv_const__rdy;
	input wire [0:0] recv_const__val;
	input wire [172:0] recv_from_ctrl_mem__msg;
	output reg [0:0] recv_from_ctrl_mem__rdy;
	input wire [0:0] recv_from_ctrl_mem__val;
	input wire [139:0] recv_in__msg;
	output reg [3:0] recv_in__rdy;
	input wire [3:0] recv_in__val;
	input wire [110:0] recv_opt__msg;
	output reg [0:0] recv_opt__rdy;
	input wire [0:0] recv_opt__val;
	output reg [69:0] send_out__msg;
	input wire [1:0] send_out__rdy;
	output reg [1:0] send_out__val;
	output reg [172:0] send_to_ctrl_mem__msg;
	input wire [0:0] send_to_ctrl_mem__rdy;
	output reg [0:0] send_to_ctrl_mem__val;
	output reg [14:0] to_mem_raddr__msg;
	input wire [0:0] to_mem_raddr__rdy;
	output reg [0:0] to_mem_raddr__val;
	output reg [14:0] to_mem_waddr__msg;
	input wire [0:0] to_mem_waddr__rdy;
	output reg [0:0] to_mem_waddr__val;
	output reg [34:0] to_mem_wdata__msg;
	input wire [0:0] to_mem_wdata__rdy;
	output reg [0:0] to_mem_wdata__val;
	localparam [34:0] const_zero = 35'h000000000;
	localparam [2:0] __const__num_inports_at_comb_logic = 3'd4;
	localparam [1:0] __const__num_outports_at_comb_logic = 2'd2;
	localparam [6:0] __const__OPT_SEL = 7'd27;
	reg [2:0] in0;
	wire [1:0] in0_idx;
	reg [2:0] in1;
	wire [1:0] in1_idx;
	reg [2:0] in2;
	wire [1:0] in2_idx;
	reg [0:0] reached_vector_factor;
	reg [0:0] recv_all_val;
	reg [7:0] vector_factor_counter;
	wire [2:0] vector_factor_power;
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : comb_logic
		if (_sv2v_0)
			;
		recv_all_val = 1'd0;
		in0 = 3'd0;
		in1 = 3'd0;
		in2 = 3'd0;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_comb_logic; i = i + 1'd1)
				recv_in__rdy[3 - sv2v_cast_2(i)+:1] = 1'd0;
		end
		recv_const__rdy = 1'd0;
		recv_opt__rdy = send_out__rdy[(1 - 1'd0) + 0+:1];
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_comb_logic; i = i + 1'd1)
				begin
					send_out__val[1 - sv2v_cast_1(i)+:1] = 1'd0;
					send_out__msg[(1 - sv2v_cast_1(i)) * 35+:35] = 35'h000000000;
				end
		end
		send_to_ctrl_mem__val = 1'd0;
		send_to_ctrl_mem__msg = 173'h00000000000000000000000000000000000000000000;
		recv_from_ctrl_mem__rdy = 1'd0;
		if (recv_opt__val) begin
			if (recv_opt__msg[92 + (2'd0 * 3)+:3] != 3'd0)
				in0 = recv_opt__msg[92 + (2'd0 * 3)+:3] - 3'd1;
			if (recv_opt__msg[92 + (2'd1 * 3)+:3] != 3'd0)
				in1 = recv_opt__msg[92 + (2'd1 * 3)+:3] - 3'd1;
			if (recv_opt__msg[92 + (2'd2 * 3)+:3] != 3'd0)
				in2 = recv_opt__msg[92 + (2'd2 * 3)+:3] - 3'd1;
		end
		if (recv_opt__val) begin
			if (recv_opt__msg[110-:7] == __const__OPT_SEL) begin
				if (recv_in__msg[((3 - in0_idx) * 35) + 34-:32] == 32'd1)
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = recv_in__msg[(3 - in1_idx) * 35+:35];
				else
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = recv_in__msg[(3 - in2_idx) * 35+:35];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = ((recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_in__msg[((3 - in1_idx) * 35) + 2]) & recv_in__msg[((3 - in2_idx) * 35) + 2]) & reached_vector_factor;
				recv_all_val = (recv_in__val[3 - in0_idx+:1] & recv_in__val[3 - in1_idx+:1]) & recv_in__val[3 - in2_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in1_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in2_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else begin
				begin : sv2v_autoblock_3
					reg [31:0] j;
					for (j = 1'd0; j < __const__num_outports_at_comb_logic; j = j + 1'd1)
						send_out__val[1 - sv2v_cast_1(j)+:1] = 1'd0;
				end
				recv_opt__rdy = 1'd0;
				recv_in__rdy[3 - in0_idx+:1] = 1'd0;
				recv_in__rdy[3 - in1_idx+:1] = 1'd0;
				recv_in__rdy[3 - in2_idx+:1] = 1'd0;
			end
		end
	end
	always @(*) begin : update_mem
		if (_sv2v_0)
			;
		to_mem_waddr__val = 1'd0;
		to_mem_wdata__val = 1'd0;
		to_mem_wdata__msg = const_zero;
		to_mem_waddr__msg = 15'd0;
		to_mem_raddr__msg = 15'd0;
		to_mem_raddr__val = 1'd0;
		from_mem_rdata__rdy = 1'd0;
	end
	always @(*) begin : update_reached_vector_factor
		if (_sv2v_0)
			;
		reached_vector_factor = 1'd0;
		if (recv_opt__val & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) >= (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
			reached_vector_factor = 1'd1;
	end
	always @(posedge clk) begin : update_vector_factor_counter
		if (reset | clear)
			vector_factor_counter <= 8'd0;
		else if (recv_opt__val) begin
			if (recv_opt__msg[40] & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) < (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
				vector_factor_counter <= vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power});
			else if (recv_opt__msg[40] & reached_vector_factor)
				vector_factor_counter <= 8'd0;
		end
	end
	assign in0_idx = in0[1:0];
	assign in1_idx = in1[1:0];
	assign in2_idx = in2[1:0];
	assign vector_factor_power = 3'd0;
	initial _sv2v_0 = 0;
endmodule
module GrantRTL__00ec249fa9189fd7 (
	clear,
	clk,
	ctrl_addr_inport,
	reset,
	from_mem_rdata__msg,
	from_mem_rdata__rdy,
	from_mem_rdata__val,
	recv_const__msg,
	recv_const__rdy,
	recv_const__val,
	recv_from_ctrl_mem__msg,
	recv_from_ctrl_mem__rdy,
	recv_from_ctrl_mem__val,
	recv_in__msg,
	recv_in__rdy,
	recv_in__val,
	recv_opt__msg,
	recv_opt__rdy,
	recv_opt__val,
	send_out__msg,
	send_out__rdy,
	send_out__val,
	send_to_ctrl_mem__msg,
	send_to_ctrl_mem__rdy,
	send_to_ctrl_mem__val,
	to_mem_raddr__msg,
	to_mem_raddr__rdy,
	to_mem_raddr__val,
	to_mem_waddr__msg,
	to_mem_waddr__rdy,
	to_mem_waddr__val,
	to_mem_wdata__msg,
	to_mem_wdata__rdy,
	to_mem_wdata__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [5:0] ctrl_addr_inport;
	input wire [0:0] reset;
	input wire [34:0] from_mem_rdata__msg;
	output reg [0:0] from_mem_rdata__rdy;
	input wire [0:0] from_mem_rdata__val;
	input wire [34:0] recv_const__msg;
	output reg [0:0] recv_const__rdy;
	input wire [0:0] recv_const__val;
	input wire [172:0] recv_from_ctrl_mem__msg;
	output reg [0:0] recv_from_ctrl_mem__rdy;
	input wire [0:0] recv_from_ctrl_mem__val;
	input wire [139:0] recv_in__msg;
	output reg [3:0] recv_in__rdy;
	input wire [3:0] recv_in__val;
	input wire [110:0] recv_opt__msg;
	output reg [0:0] recv_opt__rdy;
	input wire [0:0] recv_opt__val;
	output reg [69:0] send_out__msg;
	input wire [1:0] send_out__rdy;
	output reg [1:0] send_out__val;
	output reg [172:0] send_to_ctrl_mem__msg;
	input wire [0:0] send_to_ctrl_mem__rdy;
	output reg [0:0] send_to_ctrl_mem__val;
	output reg [14:0] to_mem_raddr__msg;
	input wire [0:0] to_mem_raddr__rdy;
	output reg [0:0] to_mem_raddr__val;
	output reg [14:0] to_mem_waddr__msg;
	input wire [0:0] to_mem_waddr__rdy;
	output reg [0:0] to_mem_waddr__val;
	output reg [34:0] to_mem_wdata__msg;
	input wire [0:0] to_mem_wdata__rdy;
	output reg [0:0] to_mem_wdata__val;
	localparam [34:0] const_zero = 35'h000000000;
	localparam [6:0] __const__OPT_START = 7'd0;
	localparam [0:0] __const__latency_at_proceed_latency = 1'd1;
	localparam [2:0] __const__num_inports_at_comb_logic = 3'd4;
	localparam [1:0] __const__num_outports_at_comb_logic = 2'd2;
	localparam [6:0] __const__OPT_GRT_PRED = 7'd16;
	localparam [6:0] __const__OPT_GRT_ALWAYS = 7'd34;
	localparam [6:0] __const__OPT_GRT_ONCE = 7'd47;
	localparam [6:0] __const__OPT_GRT_ONCE_CONST = 7'd88;
	localparam [6:0] __const__num_slots_at_record_grt_once = 7'd64;
	reg [0:0] already_grt_once [0:63];
	reg [0:0] cur_already_grt_once;
	reg [2:0] in0;
	wire [1:0] in0_idx;
	reg [2:0] in1;
	wire [1:0] in1_idx;
	reg [0:0] latency;
	reg [0:0] reached_vector_factor;
	reg [0:0] recv_all_val;
	reg [7:0] vector_factor_counter;
	wire [2:0] vector_factor_power;
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : comb_logic
		if (_sv2v_0)
			;
		recv_all_val = 1'd0;
		in0 = 3'd0;
		in1 = 3'd0;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_comb_logic; i = i + 1'd1)
				recv_in__rdy[3 - sv2v_cast_2(i)+:1] = 1'd0;
		end
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_comb_logic; i = i + 1'd1)
				begin
					send_out__val[1 - sv2v_cast_1(i)+:1] = 1'd0;
					send_out__msg[(1 - sv2v_cast_1(i)) * 35+:35] = 35'h000000000;
				end
		end
		recv_const__rdy = 1'd0;
		recv_opt__rdy = 1'd0;
		send_to_ctrl_mem__val = 1'd0;
		send_to_ctrl_mem__msg = 173'h00000000000000000000000000000000000000000000;
		recv_from_ctrl_mem__rdy = 1'd0;
		cur_already_grt_once = already_grt_once[ctrl_addr_inport];
		if (recv_opt__val) begin
			if (recv_opt__msg[92 + (2'd0 * 3)+:3] != 3'd0)
				in0 = recv_opt__msg[92 + (2'd0 * 3)+:3] - 3'd1;
			if (recv_opt__msg[92 + (2'd1 * 3)+:3] != 3'd0)
				in1 = recv_opt__msg[92 + (2'd1 * 3)+:3] - 3'd1;
		end
		if (recv_opt__val) begin
			if (recv_opt__msg[110-:7] == __const__OPT_GRT_PRED) begin
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32];
				if (recv_in__msg[((3 - in1_idx) * 35) + 34-:32] != 32'd0)
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_in__msg[((3 - in1_idx) * 35) + 2]) & reached_vector_factor;
				else
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = 1'd0;
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_in__val[3 - in1_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in1_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_GRT_ALWAYS) begin
				send_out__msg[((1 - 1'd0) + 0) * 35+:35] = recv_in__msg[(3 - in0_idx) * 35+:35];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = reached_vector_factor;
				recv_all_val = recv_in__val[3 - in0_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_GRT_ONCE) begin
				send_out__msg[((1 - 1'd0) + 0) * 35+:35] = recv_in__msg[(3 - in0_idx) * 35+:35];
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = reached_vector_factor & ~cur_already_grt_once;
				recv_all_val = recv_in__val[3 - in0_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_GRT_ONCE_CONST) begin
				send_out__msg[((1 - 1'd0) + 0) * 35+:35] = recv_const__msg;
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = reached_vector_factor & ~cur_already_grt_once;
				recv_all_val = recv_const__val;
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_const__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else begin
				begin : sv2v_autoblock_3
					reg [31:0] j;
					for (j = 1'd0; j < __const__num_outports_at_comb_logic; j = j + 1'd1)
						send_out__val[1 - sv2v_cast_1(j)+:1] = 1'd0;
				end
				recv_opt__rdy = 1'd0;
				recv_in__rdy[3 - in0_idx+:1] = 1'd0;
				recv_in__rdy[3 - in1_idx+:1] = 1'd0;
			end
		end
	end
	always @(*) begin : update_mem
		if (_sv2v_0)
			;
		to_mem_waddr__val = 1'd0;
		to_mem_wdata__val = 1'd0;
		to_mem_wdata__msg = const_zero;
		to_mem_waddr__msg = 15'd0;
		to_mem_raddr__msg = 15'd0;
		to_mem_raddr__val = 1'd0;
		from_mem_rdata__rdy = 1'd0;
	end
	always @(*) begin : update_reached_vector_factor
		if (_sv2v_0)
			;
		reached_vector_factor = 1'd0;
		if (recv_opt__val & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) >= (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
			reached_vector_factor = 1'd1;
	end
	always @(posedge clk) begin : proceed_latency
		if ((reset | clear) | (recv_opt__msg[110-:7] == __const__OPT_START))
			latency <= 1'd0;
		else if (latency == (__const__latency_at_proceed_latency - 1'd1))
			latency <= 1'd0;
		else
			latency <= latency + 1'd1;
	end
	function automatic [5:0] sv2v_cast_6;
		input reg [5:0] inp;
		sv2v_cast_6 = inp;
	endfunction
	always @(posedge clk) begin : record_grt_once
		begin : sv2v_autoblock_4
			reg [31:0] k;
			for (k = 1'd0; k < __const__num_slots_at_record_grt_once; k = k + 1'd1)
				if (reset | clear)
					already_grt_once[sv2v_cast_6(k)] <= 1'd0;
				else if (((((ctrl_addr_inport == sv2v_cast_6(k)) & ~already_grt_once[sv2v_cast_6(k)]) & send_out__val[(1 - 1'd0) + 0+:1]) & send_out__rdy[(1 - 1'd0) + 0+:1]) & ((recv_opt__msg[110-:7] == __const__OPT_GRT_ONCE) | (recv_opt__msg[110-:7] == __const__OPT_GRT_ONCE_CONST)))
					already_grt_once[sv2v_cast_6(k)] <= 1'd1;
				else
					already_grt_once[sv2v_cast_6(k)] <= already_grt_once[sv2v_cast_6(k)];
		end
	end
	always @(posedge clk) begin : update_vector_factor_counter
		if (reset | clear)
			vector_factor_counter <= 8'd0;
		else if (recv_opt__val) begin
			if (recv_opt__msg[40] & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) < (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
				vector_factor_counter <= vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power});
			else if (recv_opt__msg[40] & reached_vector_factor)
				vector_factor_counter <= 8'd0;
		end
	end
	assign vector_factor_power = 3'd0;
	assign in0_idx = in0[1:0];
	assign in1_idx = in1[1:0];
	initial _sv2v_0 = 0;
endmodule
module PhiRTL__00ec249fa9189fd7 (
	clear,
	clk,
	ctrl_addr_inport,
	reset,
	from_mem_rdata__msg,
	from_mem_rdata__rdy,
	from_mem_rdata__val,
	recv_const__msg,
	recv_const__rdy,
	recv_const__val,
	recv_from_ctrl_mem__msg,
	recv_from_ctrl_mem__rdy,
	recv_from_ctrl_mem__val,
	recv_in__msg,
	recv_in__rdy,
	recv_in__val,
	recv_opt__msg,
	recv_opt__rdy,
	recv_opt__val,
	send_out__msg,
	send_out__rdy,
	send_out__val,
	send_to_ctrl_mem__msg,
	send_to_ctrl_mem__rdy,
	send_to_ctrl_mem__val,
	to_mem_raddr__msg,
	to_mem_raddr__rdy,
	to_mem_raddr__val,
	to_mem_waddr__msg,
	to_mem_waddr__rdy,
	to_mem_waddr__val,
	to_mem_wdata__msg,
	to_mem_wdata__rdy,
	to_mem_wdata__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [5:0] ctrl_addr_inport;
	input wire [0:0] reset;
	input wire [34:0] from_mem_rdata__msg;
	output reg [0:0] from_mem_rdata__rdy;
	input wire [0:0] from_mem_rdata__val;
	input wire [34:0] recv_const__msg;
	output reg [0:0] recv_const__rdy;
	input wire [0:0] recv_const__val;
	input wire [172:0] recv_from_ctrl_mem__msg;
	output reg [0:0] recv_from_ctrl_mem__rdy;
	input wire [0:0] recv_from_ctrl_mem__val;
	input wire [139:0] recv_in__msg;
	output reg [3:0] recv_in__rdy;
	input wire [3:0] recv_in__val;
	input wire [110:0] recv_opt__msg;
	output reg [0:0] recv_opt__rdy;
	input wire [0:0] recv_opt__val;
	output reg [69:0] send_out__msg;
	input wire [1:0] send_out__rdy;
	output reg [1:0] send_out__val;
	output reg [172:0] send_to_ctrl_mem__msg;
	input wire [0:0] send_to_ctrl_mem__rdy;
	output reg [0:0] send_to_ctrl_mem__val;
	output reg [14:0] to_mem_raddr__msg;
	input wire [0:0] to_mem_raddr__rdy;
	output reg [0:0] to_mem_raddr__val;
	output reg [14:0] to_mem_waddr__msg;
	input wire [0:0] to_mem_waddr__rdy;
	output reg [0:0] to_mem_waddr__val;
	output reg [34:0] to_mem_wdata__msg;
	input wire [0:0] to_mem_wdata__rdy;
	output reg [0:0] to_mem_wdata__val;
	localparam [34:0] const_zero = 35'h000000000;
	localparam [6:0] __const__OPT_START = 7'd0;
	localparam [0:0] __const__latency_at_proceed_latency = 1'd1;
	localparam [2:0] __const__num_inports_at_comb_logic = 3'd4;
	localparam [1:0] __const__num_outports_at_comb_logic = 2'd2;
	localparam [6:0] __const__OPT_PHI = 7'd17;
	localparam [6:0] __const__OPT_PHI_START = 7'd84;
	localparam [6:0] __const__OPT_PHI_CONST = 7'd32;
	reg [0:0] first [0:63];
	reg [2:0] in0;
	wire [1:0] in0_idx;
	reg [2:0] in1;
	wire [1:0] in1_idx;
	reg [0:0] latency;
	reg [0:0] reached_vector_factor;
	reg [0:0] recv_all_val;
	reg [7:0] vector_factor_counter;
	wire [2:0] vector_factor_power;
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : comb_logic
		if (_sv2v_0)
			;
		recv_all_val = 1'd0;
		in0 = 3'd0;
		in1 = 3'd0;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_comb_logic; i = i + 1'd1)
				recv_in__rdy[3 - sv2v_cast_2(i)+:1] = 1'd0;
		end
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_comb_logic; i = i + 1'd1)
				begin
					send_out__val[1 - sv2v_cast_1(i)+:1] = 1'd0;
					send_out__msg[(1 - sv2v_cast_1(i)) * 35+:35] = 35'h000000000;
				end
		end
		recv_const__rdy = 1'd0;
		recv_opt__rdy = 1'd0;
		send_to_ctrl_mem__val = 1'd0;
		send_to_ctrl_mem__msg = 173'h00000000000000000000000000000000000000000000;
		recv_from_ctrl_mem__rdy = 1'd0;
		if (recv_opt__val) begin
			if (recv_opt__msg[92 + (2'd0 * 3)+:3] != 3'd0)
				in0 = recv_opt__msg[92 + (2'd0 * 3)+:3] - 3'd1;
			if (recv_opt__msg[92 + (2'd1 * 3)+:3] != 3'd0)
				in1 = recv_opt__msg[92 + (2'd1 * 3)+:3] - 3'd1;
		end
		if (recv_opt__val) begin
			if (recv_opt__msg[110-:7] == __const__OPT_PHI) begin
				if (recv_in__msg[((3 - in0_idx) * 35) + 2] == 1'd1) begin
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32];
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = reached_vector_factor;
				end
				else if (recv_in__msg[((3 - in1_idx) * 35) + 2] == 1'd1) begin
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in1_idx) * 35) + 34-:32];
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = reached_vector_factor;
				end
				else begin
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32];
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = 1'd0;
				end
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_in__val[3 - in1_idx+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in1_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_PHI_START) begin
				if (first[ctrl_addr_inport]) begin
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32];
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = reached_vector_factor;
				end
				else if (recv_in__msg[((3 - in0_idx) * 35) + 2] == 1'd1) begin
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32];
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = reached_vector_factor;
				end
				else if (recv_in__msg[((3 - in1_idx) * 35) + 2] == 1'd1) begin
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in1_idx) * 35) + 34-:32];
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = reached_vector_factor;
				end
				else begin
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32];
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = 1'd0;
				end
				recv_all_val = (first[ctrl_addr_inport] & recv_in__val[3 - in0_idx+:1]) | ((~first[ctrl_addr_inport] & recv_in__val[3 - in0_idx+:1]) & recv_in__val[3 - in1_idx+:1]);
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_in__rdy[3 - in1_idx+:1] = (~first[ctrl_addr_inport] & recv_all_val) & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_PHI_CONST) begin
				if (first[ctrl_addr_inport])
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_const__msg[34-:32];
				else
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 34-:32] = recv_in__msg[((3 - in0_idx) * 35) + 34-:32];
				recv_all_val = (first[ctrl_addr_inport] & recv_const__val) | (~first[ctrl_addr_inport] & recv_in__val[3 - in0_idx+:1]);
				send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_const__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				recv_opt__rdy = recv_all_val & send_out__rdy[(1 - 1'd0) + 0+:1];
				if (first[ctrl_addr_inport])
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = recv_const__msg[2] & reached_vector_factor;
				else
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = recv_in__msg[((3 - in0_idx) * 35) + 2] & reached_vector_factor;
			end
			else begin
				begin : sv2v_autoblock_3
					reg [31:0] j;
					for (j = 1'd0; j < __const__num_outports_at_comb_logic; j = j + 1'd1)
						send_out__val[1 - sv2v_cast_1(j)+:1] = 1'd0;
				end
				recv_opt__rdy = 1'd0;
				recv_in__rdy[3 - in0_idx+:1] = 1'd0;
				recv_in__rdy[3 - in1_idx+:1] = 1'd0;
			end
		end
	end
	always @(*) begin : update_mem
		if (_sv2v_0)
			;
		to_mem_waddr__val = 1'd0;
		to_mem_wdata__val = 1'd0;
		to_mem_wdata__msg = const_zero;
		to_mem_waddr__msg = 15'd0;
		to_mem_raddr__msg = 15'd0;
		to_mem_raddr__val = 1'd0;
		from_mem_rdata__rdy = 1'd0;
	end
	always @(*) begin : update_reached_vector_factor
		if (_sv2v_0)
			;
		reached_vector_factor = 1'd0;
		if (recv_opt__val & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) >= (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
			reached_vector_factor = 1'd1;
	end
	always @(posedge clk) begin : proceed_latency
		if ((reset | clear) | (recv_opt__msg[110-:7] == __const__OPT_START))
			latency <= 1'd0;
		else if (latency == (__const__latency_at_proceed_latency - 1'd1))
			latency <= 1'd0;
		else
			latency <= latency + 1'd1;
	end
	function automatic [5:0] sv2v_cast_6;
		input reg [5:0] inp;
		sv2v_cast_6 = inp;
	endfunction
	always @(posedge clk) begin : record_first_execution
		if (reset | clear) begin : sv2v_autoblock_4
			reg [31:0] i;
			for (i = 1'd0; i < (3'd2 ** 3'd6); i = i + 1'd1)
				first[sv2v_cast_6(i)] <= 1'd1;
		end
		if (((recv_opt__msg[110-:7] == __const__OPT_PHI_CONST) | (recv_opt__msg[110-:7] == __const__OPT_PHI_START)) & reached_vector_factor)
			first[ctrl_addr_inport] <= 1'd0;
	end
	always @(posedge clk) begin : update_vector_factor_counter
		if (reset | clear)
			vector_factor_counter <= 8'd0;
		else if (recv_opt__val) begin
			if (recv_opt__msg[40] & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) < (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
				vector_factor_counter <= vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power});
			else if (recv_opt__msg[40] & reached_vector_factor)
				vector_factor_counter <= 8'd0;
		end
	end
	assign vector_factor_power = 3'd0;
	assign in0_idx = in0[1:0];
	assign in1_idx = in1[1:0];
	initial _sv2v_0 = 0;
endmodule
module MemUnitRTL__00ec249fa9189fd7 (
	clear,
	clk,
	ctrl_addr_inport,
	reset,
	from_mem_rdata__msg,
	from_mem_rdata__rdy,
	from_mem_rdata__val,
	recv_const__msg,
	recv_const__rdy,
	recv_const__val,
	recv_from_ctrl_mem__msg,
	recv_from_ctrl_mem__rdy,
	recv_from_ctrl_mem__val,
	recv_in__msg,
	recv_in__rdy,
	recv_in__val,
	recv_opt__msg,
	recv_opt__rdy,
	recv_opt__val,
	send_out__msg,
	send_out__rdy,
	send_out__val,
	send_to_ctrl_mem__msg,
	send_to_ctrl_mem__rdy,
	send_to_ctrl_mem__val,
	to_mem_raddr__msg,
	to_mem_raddr__rdy,
	to_mem_raddr__val,
	to_mem_waddr__msg,
	to_mem_waddr__rdy,
	to_mem_waddr__val,
	to_mem_wdata__msg,
	to_mem_wdata__rdy,
	to_mem_wdata__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [5:0] ctrl_addr_inport;
	input wire [0:0] reset;
	input wire [34:0] from_mem_rdata__msg;
	output reg [0:0] from_mem_rdata__rdy;
	input wire [0:0] from_mem_rdata__val;
	input wire [34:0] recv_const__msg;
	output reg [0:0] recv_const__rdy;
	input wire [0:0] recv_const__val;
	input wire [172:0] recv_from_ctrl_mem__msg;
	output reg [0:0] recv_from_ctrl_mem__rdy;
	input wire [0:0] recv_from_ctrl_mem__val;
	input wire [139:0] recv_in__msg;
	output reg [3:0] recv_in__rdy;
	input wire [3:0] recv_in__val;
	input wire [110:0] recv_opt__msg;
	output reg [0:0] recv_opt__rdy;
	input wire [0:0] recv_opt__val;
	output reg [69:0] send_out__msg;
	input wire [1:0] send_out__rdy;
	output reg [1:0] send_out__val;
	output reg [172:0] send_to_ctrl_mem__msg;
	input wire [0:0] send_to_ctrl_mem__rdy;
	output reg [0:0] send_to_ctrl_mem__val;
	output reg [14:0] to_mem_raddr__msg;
	input wire [0:0] to_mem_raddr__rdy;
	output reg [0:0] to_mem_raddr__val;
	output reg [14:0] to_mem_waddr__msg;
	input wire [0:0] to_mem_waddr__rdy;
	output reg [0:0] to_mem_waddr__val;
	output reg [34:0] to_mem_wdata__msg;
	input wire [0:0] to_mem_wdata__rdy;
	output reg [0:0] to_mem_wdata__val;
	localparam [2:0] __const__num_inports_at_comb_logic = 3'd4;
	localparam [1:0] __const__num_outports_at_comb_logic = 2'd2;
	localparam [6:0] __const__OPT_LD = 7'd12;
	localparam [6:0] __const__OPT_ADD_CONST_LD = 7'd81;
	localparam [6:0] __const__OPT_LD_CONST = 7'd28;
	localparam [6:0] __const__OPT_STR = 7'd13;
	localparam [6:0] __const__OPT_STR_CONST = 7'd58;
	reg [0:0] already_sent_raddr;
	reg [2:0] in0;
	wire [1:0] in0_idx;
	reg [2:0] in1;
	wire [1:0] in1_idx;
	reg [0:0] reached_vector_factor;
	reg [0:0] recv_all_val;
	wire [3:0] recv_in_val_vector;
	reg [7:0] vector_factor_counter;
	wire [2:0] vector_factor_power;
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	function automatic [14:0] sv2v_cast_15;
		input reg [14:0] inp;
		sv2v_cast_15 = inp;
	endfunction
	always @(*) begin : comb_logic
		if (_sv2v_0)
			;
		recv_all_val = 1'd0;
		in0 = 3'd0;
		in1 = 3'd0;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_comb_logic; i = i + 1'd1)
				recv_in__rdy[3 - sv2v_cast_2(i)+:1] = 1'd0;
		end
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_comb_logic; i = i + 1'd1)
				begin
					send_out__val[1 - sv2v_cast_1(i)+:1] = 1'd0;
					send_out__msg[(1 - sv2v_cast_1(i)) * 35+:35] = 35'h000000000;
				end
		end
		recv_const__rdy = 1'd0;
		recv_opt__rdy = 1'd0;
		send_to_ctrl_mem__val = 1'd0;
		send_to_ctrl_mem__msg = 173'h00000000000000000000000000000000000000000000;
		recv_from_ctrl_mem__rdy = 1'd0;
		if (recv_opt__val) begin
			if (recv_opt__msg[92 + (2'd0 * 3)+:3] != 3'd0)
				in0 = recv_opt__msg[92 + (2'd0 * 3)+:3] - 3'd1;
			if (recv_opt__msg[92 + (2'd1 * 3)+:3] != 3'd0)
				in1 = recv_opt__msg[92 + (2'd1 * 3)+:3] - 3'd1;
		end
		to_mem_waddr__val = 1'd0;
		to_mem_waddr__msg = 15'd0;
		to_mem_wdata__val = 1'd0;
		to_mem_wdata__msg = 35'h000000000;
		to_mem_raddr__val = 1'd0;
		to_mem_raddr__msg = 15'd0;
		from_mem_rdata__rdy = 1'd0;
		if (recv_opt__val) begin
			if (recv_opt__msg[110-:7] == __const__OPT_LD) begin
				recv_all_val = recv_in__val[3 - in0_idx+:1];
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & to_mem_raddr__rdy;
				to_mem_raddr__msg = sv2v_cast_15(recv_in__msg[((3 - in0_idx) * 35) + ((3 + 5'd14) >= (3 + 5'd0) ? 3 + 5'd14 : ((3 + 5'd14) + ((3 + 5'd14) >= (3 + 5'd0) ? ((3 + 5'd14) - (3 + 5'd0)) + 1 : ((3 + 5'd0) - (3 + 5'd14)) + 1)) - 1)-:((3 + 5'd14) >= (3 + 5'd0) ? ((3 + 5'd14) - (3 + 5'd0)) + 1 : ((3 + 5'd0) - (3 + 5'd14)) + 1)]);
				if (recv_all_val & (recv_in__msg[((3 - in0_idx) * 35) + 2] == 1'd0))
					to_mem_raddr__val = 1'd0;
				else
					to_mem_raddr__val = recv_all_val & ~already_sent_raddr;
				from_mem_rdata__rdy = send_out__rdy[(1 - 1'd0) + 0+:1];
				if (recv_all_val & (recv_in__msg[((3 - in0_idx) * 35) + 2] == 1'd0)) begin
					send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = 1'd0;
					recv_opt__rdy = send_out__rdy[(1 - 1'd0) + 0+:1];
				end
				else begin
					send_out__val[(1 - 1'd0) + 0+:1] = from_mem_rdata__val;
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = from_mem_rdata__msg;
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = from_mem_rdata__msg[2] & reached_vector_factor;
					recv_opt__rdy = send_out__rdy[(1 - 1'd0) + 0+:1] & from_mem_rdata__val;
				end
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_ADD_CONST_LD) begin
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_const__val;
				recv_in__rdy[3 - in0_idx+:1] = recv_all_val & to_mem_raddr__rdy;
				recv_const__rdy = 1'd1;
				to_mem_raddr__msg = sv2v_cast_15(recv_in__msg[((3 - in0_idx) * 35) + ((3 + 5'd14) >= (3 + 5'd0) ? 3 + 5'd14 : ((3 + 5'd14) + ((3 + 5'd14) >= (3 + 5'd0) ? ((3 + 5'd14) - (3 + 5'd0)) + 1 : ((3 + 5'd0) - (3 + 5'd14)) + 1)) - 1)-:((3 + 5'd14) >= (3 + 5'd0) ? ((3 + 5'd14) - (3 + 5'd0)) + 1 : ((3 + 5'd0) - (3 + 5'd14)) + 1)] + recv_const__msg[3 + 5'd14:3 + 5'd0]);
				if (recv_all_val & (recv_in__msg[((3 - in0_idx) * 35) + 2] == 1'd0))
					to_mem_raddr__val = 1'd0;
				else
					to_mem_raddr__val = recv_all_val & ~already_sent_raddr;
				from_mem_rdata__rdy = send_out__rdy[(1 - 1'd0) + 0+:1];
				if (recv_all_val & (recv_in__msg[((3 - in0_idx) * 35) + 2] == 1'd0)) begin
					send_out__val[(1 - 1'd0) + 0+:1] = recv_all_val;
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = 1'd0;
					recv_opt__rdy = send_out__rdy[(1 - 1'd0) + 0+:1];
				end
				else begin
					send_out__val[(1 - 1'd0) + 0+:1] = from_mem_rdata__val;
					send_out__msg[((1 - 1'd0) + 0) * 35+:35] = from_mem_rdata__msg;
					send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = from_mem_rdata__msg[2] & reached_vector_factor;
					recv_opt__rdy = send_out__rdy[(1 - 1'd0) + 0+:1] & from_mem_rdata__val;
				end
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_LD_CONST) begin
				recv_all_val = recv_const__val;
				recv_const__rdy = 1'd1;
				to_mem_raddr__msg = sv2v_cast_15(recv_const__msg[3 + 5'd14:3 + 5'd0]);
				to_mem_raddr__val = recv_all_val & ~already_sent_raddr;
				from_mem_rdata__rdy = send_out__rdy[(1 - 1'd0) + 0+:1];
				send_out__val[(1 - 1'd0) + 0+:1] = from_mem_rdata__val;
				send_out__msg[((1 - 1'd0) + 0) * 35+:35] = from_mem_rdata__msg;
				send_out__msg[(((1 - 1'd0) + 0) * 35) + 2] = (recv_const__msg[2] & from_mem_rdata__msg[2]) & reached_vector_factor;
				recv_opt__rdy = send_out__rdy[(1 - 1'd0) + 0+:1] & from_mem_rdata__val;
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_STR) begin
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_in__val[3 - in1_idx+:1];
				recv_in__rdy[3 - in0_idx+:1] = (recv_all_val & to_mem_waddr__rdy) & to_mem_wdata__rdy;
				recv_in__rdy[3 - in1_idx+:1] = (recv_all_val & to_mem_waddr__rdy) & to_mem_wdata__rdy;
				to_mem_waddr__msg = sv2v_cast_15(recv_in__msg[((3 - in0_idx) * 35) + ((3 + 5'd14) >= (3 + 5'd0) ? 3 + 5'd14 : ((3 + 5'd14) + ((3 + 5'd14) >= (3 + 5'd0) ? ((3 + 5'd14) - (3 + 5'd0)) + 1 : ((3 + 5'd0) - (3 + 5'd14)) + 1)) - 1)-:((3 + 5'd14) >= (3 + 5'd0) ? ((3 + 5'd14) - (3 + 5'd0)) + 1 : ((3 + 5'd0) - (3 + 5'd14)) + 1)]);
				to_mem_waddr__val = (recv_all_val & recv_in__msg[((3 - in0_idx) * 35) + 2]) & recv_in__msg[((3 - in1_idx) * 35) + 2];
				to_mem_wdata__msg = recv_in__msg[(3 - in1_idx) * 35+:35];
				to_mem_wdata__msg[2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_in__msg[((3 - in1_idx) * 35) + 2]) & reached_vector_factor;
				to_mem_wdata__val = (recv_all_val & recv_in__msg[((3 - in0_idx) * 35) + 2]) & recv_in__msg[((3 - in1_idx) * 35) + 2];
				send_out__val[(1 - 1'd0) + 0+:1] = 1'd0;
				recv_opt__rdy = (recv_all_val & to_mem_waddr__rdy) & to_mem_wdata__rdy;
			end
			else if (recv_opt__msg[110-:7] == __const__OPT_STR_CONST) begin
				recv_all_val = recv_in__val[3 - in0_idx+:1] & recv_const__val;
				recv_const__rdy = (recv_all_val & to_mem_waddr__rdy) & to_mem_wdata__rdy;
				recv_in__rdy[3 - in0_idx+:1] = (recv_all_val & to_mem_waddr__rdy) & to_mem_wdata__rdy;
				to_mem_waddr__msg = sv2v_cast_15(recv_const__msg[3 + 5'd14:3 + 5'd0]);
				to_mem_waddr__val = (recv_all_val & recv_in__msg[((3 - in0_idx) * 35) + 2]) & recv_const__msg[2];
				to_mem_wdata__msg = recv_in__msg[(3 - in0_idx) * 35+:35];
				to_mem_wdata__msg[2] = (recv_in__msg[((3 - in0_idx) * 35) + 2] & recv_const__msg[2]) & reached_vector_factor;
				to_mem_wdata__val = (recv_all_val & recv_in__msg[((3 - in0_idx) * 35) + 2]) & recv_const__msg[2];
				send_out__val[(1 - 1'd0) + 0+:1] = 1'd0;
				recv_opt__rdy = (recv_all_val & to_mem_waddr__rdy) & to_mem_wdata__rdy;
			end
			else begin
				begin : sv2v_autoblock_3
					reg [31:0] j;
					for (j = 1'd0; j < __const__num_outports_at_comb_logic; j = j + 1'd1)
						send_out__val[1 - sv2v_cast_1(j)+:1] = 1'd0;
				end
				recv_opt__rdy = 1'd0;
				recv_in__rdy[3 - in0_idx+:1] = 1'd0;
				recv_in__rdy[3 - in1_idx+:1] = 1'd0;
			end
		end
	end
	always @(*) begin : update_reached_vector_factor
		if (_sv2v_0)
			;
		reached_vector_factor = 1'd0;
		if (recv_opt__val & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) >= (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
			reached_vector_factor = 1'd1;
	end
	always @(posedge clk) begin : update_already_sent_raddr
		if (reset | clear)
			already_sent_raddr <= 1'd0;
		else if (~recv_opt__val)
			already_sent_raddr <= 1'd0;
		else if (from_mem_rdata__val & from_mem_rdata__rdy)
			already_sent_raddr <= 1'd0;
		else if ((to_mem_raddr__val & to_mem_raddr__rdy) & ~already_sent_raddr)
			already_sent_raddr <= 1'd1;
		else
			already_sent_raddr <= already_sent_raddr;
	end
	always @(posedge clk) begin : update_vector_factor_counter
		if (reset | clear)
			vector_factor_counter <= 8'd0;
		else if (recv_opt__val) begin
			if (recv_opt__msg[40] & ((vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power})) < (8'd1 << {{5 {1'b0}}, recv_opt__msg[43-:3]})))
				vector_factor_counter <= vector_factor_counter + (8'd1 << {{5 {1'b0}}, vector_factor_power});
			else if (recv_opt__msg[40] & reached_vector_factor)
				vector_factor_counter <= 8'd0;
		end
	end
	assign in0_idx = in0[1:0];
	assign in1_idx = in1[1:0];
	assign vector_factor_power = 3'd0;
	initial _sv2v_0 = 0;
endmodule
module FlexibleFuRTL__8889a6e6a7910850 (
	clear,
	clk,
	ctrl_addr_inport,
	prologue_count_inport,
	reset,
	tile_id,
	from_mem_rdata__msg,
	from_mem_rdata__rdy,
	from_mem_rdata__val,
	recv_const__msg,
	recv_const__rdy,
	recv_const__val,
	recv_from_ctrl_mem__msg,
	recv_from_ctrl_mem__rdy,
	recv_from_ctrl_mem__val,
	recv_in__msg,
	recv_in__rdy,
	recv_in__val,
	recv_opt__msg,
	recv_opt__rdy,
	recv_opt__val,
	recv_pkt_from_controller__msg,
	recv_pkt_from_controller__rdy,
	recv_pkt_from_controller__val,
	send_out__msg,
	send_out__rdy,
	send_out__val,
	send_to_ctrl_mem__msg,
	send_to_ctrl_mem__rdy,
	send_to_ctrl_mem__val,
	to_mem_raddr__msg,
	to_mem_raddr__rdy,
	to_mem_raddr__val,
	to_mem_waddr__msg,
	to_mem_waddr__rdy,
	to_mem_waddr__val,
	to_mem_wdata__msg,
	to_mem_wdata__rdy,
	to_mem_wdata__val
);
	reg _sv2v_0;
	input wire [12:0] clear;
	input wire [0:0] clk;
	input wire [5:0] ctrl_addr_inport;
	input wire [2:0] prologue_count_inport;
	input wire [0:0] reset;
	input wire [4:0] tile_id;
	input wire [454:0] from_mem_rdata__msg;
	output wire [12:0] from_mem_rdata__rdy;
	input wire [12:0] from_mem_rdata__val;
	input wire [34:0] recv_const__msg;
	output reg [0:0] recv_const__rdy;
	input wire [0:0] recv_const__val;
	input wire [172:0] recv_from_ctrl_mem__msg;
	output reg [0:0] recv_from_ctrl_mem__rdy;
	input wire [0:0] recv_from_ctrl_mem__val;
	input wire [139:0] recv_in__msg;
	output reg [3:0] recv_in__rdy;
	input wire [3:0] recv_in__val;
	input wire [110:0] recv_opt__msg;
	output reg [0:0] recv_opt__rdy;
	input wire [0:0] recv_opt__val;
	input wire [197:0] recv_pkt_from_controller__msg;
	output wire [0:0] recv_pkt_from_controller__rdy;
	input wire [0:0] recv_pkt_from_controller__val;
	output reg [69:0] send_out__msg;
	input wire [1:0] send_out__rdy;
	output reg [1:0] send_out__val;
	output reg [172:0] send_to_ctrl_mem__msg;
	input wire [0:0] send_to_ctrl_mem__rdy;
	output reg [0:0] send_to_ctrl_mem__val;
	output wire [194:0] to_mem_raddr__msg;
	input wire [12:0] to_mem_raddr__rdy;
	output wire [12:0] to_mem_raddr__val;
	output wire [194:0] to_mem_waddr__msg;
	input wire [12:0] to_mem_waddr__rdy;
	output wire [12:0] to_mem_waddr__val;
	output wire [454:0] to_mem_wdata__msg;
	input wire [12:0] to_mem_wdata__rdy;
	output wire [12:0] to_mem_wdata__val;
	localparam [1:0] __const__num_outports_at_comb_logic = 2'd2;
	localparam [6:0] __const__OPT_NAH = 7'd1;
	localparam [2:0] __const__num_inports_at_comb_logic = 3'd4;
	reg [12:0] fu_recv_const_rdy_vector;
	reg [12:0] fu_recv_in_rdy_vector [0:3];
	reg [12:0] fu_recv_opt_rdy_vector;
	reg [12:0] recv_from_controller_rdy_vector;
	wire [0:0] fu__clear [0:12];
	wire [0:0] fu__clk [0:12];
	reg [5:0] fu__ctrl_addr_inport [0:12];
	wire [0:0] fu__reset [0:12];
	wire [34:0] fu__from_mem_rdata__msg [0:12];
	wire [0:0] fu__from_mem_rdata__rdy [0:12];
	wire [0:0] fu__from_mem_rdata__val [0:12];
	reg [34:0] fu__recv_const__msg [0:12];
	wire [0:0] fu__recv_const__rdy [0:12];
	reg [0:0] fu__recv_const__val [0:12];
	reg [172:0] fu__recv_from_ctrl_mem__msg [0:12];
	wire [0:0] fu__recv_from_ctrl_mem__rdy [0:12];
	reg [0:0] fu__recv_from_ctrl_mem__val [0:12];
	reg [139:0] fu__recv_in__msg [0:12];
	wire [3:0] fu__recv_in__rdy [0:12];
	reg [3:0] fu__recv_in__val [0:12];
	reg [110:0] fu__recv_opt__msg [0:12];
	wire [0:0] fu__recv_opt__rdy [0:12];
	reg [0:0] fu__recv_opt__val [0:12];
	wire [69:0] fu__send_out__msg [0:12];
	reg [1:0] fu__send_out__rdy [0:12];
	wire [1:0] fu__send_out__val [0:12];
	wire [172:0] fu__send_to_ctrl_mem__msg [0:12];
	reg [0:0] fu__send_to_ctrl_mem__rdy [0:12];
	wire [0:0] fu__send_to_ctrl_mem__val [0:12];
	wire [14:0] fu__to_mem_raddr__msg [0:12];
	wire [0:0] fu__to_mem_raddr__rdy [0:12];
	wire [0:0] fu__to_mem_raddr__val [0:12];
	wire [14:0] fu__to_mem_waddr__msg [0:12];
	wire [0:0] fu__to_mem_waddr__rdy [0:12];
	wire [0:0] fu__to_mem_waddr__val [0:12];
	wire [34:0] fu__to_mem_wdata__msg [0:12];
	wire [0:0] fu__to_mem_wdata__rdy [0:12];
	wire [0:0] fu__to_mem_wdata__val [0:12];
	AdderRTL__00ec249fa9189fd7 fu__0(
		.clear(fu__clear[0]),
		.clk(fu__clk[0]),
		.ctrl_addr_inport(fu__ctrl_addr_inport[0]),
		.reset(fu__reset[0]),
		.from_mem_rdata__msg(fu__from_mem_rdata__msg[0]),
		.from_mem_rdata__rdy(fu__from_mem_rdata__rdy[0]),
		.from_mem_rdata__val(fu__from_mem_rdata__val[0]),
		.recv_const__msg(fu__recv_const__msg[0]),
		.recv_const__rdy(fu__recv_const__rdy[0]),
		.recv_const__val(fu__recv_const__val[0]),
		.recv_from_ctrl_mem__msg(fu__recv_from_ctrl_mem__msg[0]),
		.recv_from_ctrl_mem__rdy(fu__recv_from_ctrl_mem__rdy[0]),
		.recv_from_ctrl_mem__val(fu__recv_from_ctrl_mem__val[0]),
		.recv_in__msg(fu__recv_in__msg[0]),
		.recv_in__rdy(fu__recv_in__rdy[0]),
		.recv_in__val(fu__recv_in__val[0]),
		.recv_opt__msg(fu__recv_opt__msg[0]),
		.recv_opt__rdy(fu__recv_opt__rdy[0]),
		.recv_opt__val(fu__recv_opt__val[0]),
		.send_out__msg(fu__send_out__msg[0]),
		.send_out__rdy(fu__send_out__rdy[0]),
		.send_out__val(fu__send_out__val[0]),
		.send_to_ctrl_mem__msg(fu__send_to_ctrl_mem__msg[0]),
		.send_to_ctrl_mem__rdy(fu__send_to_ctrl_mem__rdy[0]),
		.send_to_ctrl_mem__val(fu__send_to_ctrl_mem__val[0]),
		.to_mem_raddr__msg(fu__to_mem_raddr__msg[0]),
		.to_mem_raddr__rdy(fu__to_mem_raddr__rdy[0]),
		.to_mem_raddr__val(fu__to_mem_raddr__val[0]),
		.to_mem_waddr__msg(fu__to_mem_waddr__msg[0]),
		.to_mem_waddr__rdy(fu__to_mem_waddr__rdy[0]),
		.to_mem_waddr__val(fu__to_mem_waddr__val[0]),
		.to_mem_wdata__msg(fu__to_mem_wdata__msg[0]),
		.to_mem_wdata__rdy(fu__to_mem_wdata__rdy[0]),
		.to_mem_wdata__val(fu__to_mem_wdata__val[0])
	);
	MulRTL__00ec249fa9189fd7 fu__1(
		.clear(fu__clear[1]),
		.clk(fu__clk[1]),
		.ctrl_addr_inport(fu__ctrl_addr_inport[1]),
		.reset(fu__reset[1]),
		.from_mem_rdata__msg(fu__from_mem_rdata__msg[1]),
		.from_mem_rdata__rdy(fu__from_mem_rdata__rdy[1]),
		.from_mem_rdata__val(fu__from_mem_rdata__val[1]),
		.recv_const__msg(fu__recv_const__msg[1]),
		.recv_const__rdy(fu__recv_const__rdy[1]),
		.recv_const__val(fu__recv_const__val[1]),
		.recv_from_ctrl_mem__msg(fu__recv_from_ctrl_mem__msg[1]),
		.recv_from_ctrl_mem__rdy(fu__recv_from_ctrl_mem__rdy[1]),
		.recv_from_ctrl_mem__val(fu__recv_from_ctrl_mem__val[1]),
		.recv_in__msg(fu__recv_in__msg[1]),
		.recv_in__rdy(fu__recv_in__rdy[1]),
		.recv_in__val(fu__recv_in__val[1]),
		.recv_opt__msg(fu__recv_opt__msg[1]),
		.recv_opt__rdy(fu__recv_opt__rdy[1]),
		.recv_opt__val(fu__recv_opt__val[1]),
		.send_out__msg(fu__send_out__msg[1]),
		.send_out__rdy(fu__send_out__rdy[1]),
		.send_out__val(fu__send_out__val[1]),
		.send_to_ctrl_mem__msg(fu__send_to_ctrl_mem__msg[1]),
		.send_to_ctrl_mem__rdy(fu__send_to_ctrl_mem__rdy[1]),
		.send_to_ctrl_mem__val(fu__send_to_ctrl_mem__val[1]),
		.to_mem_raddr__msg(fu__to_mem_raddr__msg[1]),
		.to_mem_raddr__rdy(fu__to_mem_raddr__rdy[1]),
		.to_mem_raddr__val(fu__to_mem_raddr__val[1]),
		.to_mem_waddr__msg(fu__to_mem_waddr__msg[1]),
		.to_mem_waddr__rdy(fu__to_mem_waddr__rdy[1]),
		.to_mem_waddr__val(fu__to_mem_waddr__val[1]),
		.to_mem_wdata__msg(fu__to_mem_wdata__msg[1]),
		.to_mem_wdata__rdy(fu__to_mem_wdata__rdy[1]),
		.to_mem_wdata__val(fu__to_mem_wdata__val[1])
	);
	ExclusiveDivRTL__00ec249fa9189fd7 fu__2(
		.clear(fu__clear[2]),
		.clk(fu__clk[2]),
		.ctrl_addr_inport(fu__ctrl_addr_inport[2]),
		.reset(fu__reset[2]),
		.from_mem_rdata__msg(fu__from_mem_rdata__msg[2]),
		.from_mem_rdata__rdy(fu__from_mem_rdata__rdy[2]),
		.from_mem_rdata__val(fu__from_mem_rdata__val[2]),
		.recv_const__msg(fu__recv_const__msg[2]),
		.recv_const__rdy(fu__recv_const__rdy[2]),
		.recv_const__val(fu__recv_const__val[2]),
		.recv_from_ctrl_mem__msg(fu__recv_from_ctrl_mem__msg[2]),
		.recv_from_ctrl_mem__rdy(fu__recv_from_ctrl_mem__rdy[2]),
		.recv_from_ctrl_mem__val(fu__recv_from_ctrl_mem__val[2]),
		.recv_in__msg(fu__recv_in__msg[2]),
		.recv_in__rdy(fu__recv_in__rdy[2]),
		.recv_in__val(fu__recv_in__val[2]),
		.recv_opt__msg(fu__recv_opt__msg[2]),
		.recv_opt__rdy(fu__recv_opt__rdy[2]),
		.recv_opt__val(fu__recv_opt__val[2]),
		.send_out__msg(fu__send_out__msg[2]),
		.send_out__rdy(fu__send_out__rdy[2]),
		.send_out__val(fu__send_out__val[2]),
		.send_to_ctrl_mem__msg(fu__send_to_ctrl_mem__msg[2]),
		.send_to_ctrl_mem__rdy(fu__send_to_ctrl_mem__rdy[2]),
		.send_to_ctrl_mem__val(fu__send_to_ctrl_mem__val[2]),
		.to_mem_raddr__msg(fu__to_mem_raddr__msg[2]),
		.to_mem_raddr__rdy(fu__to_mem_raddr__rdy[2]),
		.to_mem_raddr__val(fu__to_mem_raddr__val[2]),
		.to_mem_waddr__msg(fu__to_mem_waddr__msg[2]),
		.to_mem_waddr__rdy(fu__to_mem_waddr__rdy[2]),
		.to_mem_waddr__val(fu__to_mem_waddr__val[2]),
		.to_mem_wdata__msg(fu__to_mem_wdata__msg[2]),
		.to_mem_wdata__rdy(fu__to_mem_wdata__rdy[2]),
		.to_mem_wdata__val(fu__to_mem_wdata__val[2])
	);
	LogicRTL__00ec249fa9189fd7 fu__3(
		.clear(fu__clear[3]),
		.clk(fu__clk[3]),
		.ctrl_addr_inport(fu__ctrl_addr_inport[3]),
		.reset(fu__reset[3]),
		.from_mem_rdata__msg(fu__from_mem_rdata__msg[3]),
		.from_mem_rdata__rdy(fu__from_mem_rdata__rdy[3]),
		.from_mem_rdata__val(fu__from_mem_rdata__val[3]),
		.recv_const__msg(fu__recv_const__msg[3]),
		.recv_const__rdy(fu__recv_const__rdy[3]),
		.recv_const__val(fu__recv_const__val[3]),
		.recv_from_ctrl_mem__msg(fu__recv_from_ctrl_mem__msg[3]),
		.recv_from_ctrl_mem__rdy(fu__recv_from_ctrl_mem__rdy[3]),
		.recv_from_ctrl_mem__val(fu__recv_from_ctrl_mem__val[3]),
		.recv_in__msg(fu__recv_in__msg[3]),
		.recv_in__rdy(fu__recv_in__rdy[3]),
		.recv_in__val(fu__recv_in__val[3]),
		.recv_opt__msg(fu__recv_opt__msg[3]),
		.recv_opt__rdy(fu__recv_opt__rdy[3]),
		.recv_opt__val(fu__recv_opt__val[3]),
		.send_out__msg(fu__send_out__msg[3]),
		.send_out__rdy(fu__send_out__rdy[3]),
		.send_out__val(fu__send_out__val[3]),
		.send_to_ctrl_mem__msg(fu__send_to_ctrl_mem__msg[3]),
		.send_to_ctrl_mem__rdy(fu__send_to_ctrl_mem__rdy[3]),
		.send_to_ctrl_mem__val(fu__send_to_ctrl_mem__val[3]),
		.to_mem_raddr__msg(fu__to_mem_raddr__msg[3]),
		.to_mem_raddr__rdy(fu__to_mem_raddr__rdy[3]),
		.to_mem_raddr__val(fu__to_mem_raddr__val[3]),
		.to_mem_waddr__msg(fu__to_mem_waddr__msg[3]),
		.to_mem_waddr__rdy(fu__to_mem_waddr__rdy[3]),
		.to_mem_waddr__val(fu__to_mem_waddr__val[3]),
		.to_mem_wdata__msg(fu__to_mem_wdata__msg[3]),
		.to_mem_wdata__rdy(fu__to_mem_wdata__rdy[3]),
		.to_mem_wdata__val(fu__to_mem_wdata__val[3])
	);
	ShifterRTL__00ec249fa9189fd7 fu__4(
		.clear(fu__clear[4]),
		.clk(fu__clk[4]),
		.ctrl_addr_inport(fu__ctrl_addr_inport[4]),
		.reset(fu__reset[4]),
		.from_mem_rdata__msg(fu__from_mem_rdata__msg[4]),
		.from_mem_rdata__rdy(fu__from_mem_rdata__rdy[4]),
		.from_mem_rdata__val(fu__from_mem_rdata__val[4]),
		.recv_const__msg(fu__recv_const__msg[4]),
		.recv_const__rdy(fu__recv_const__rdy[4]),
		.recv_const__val(fu__recv_const__val[4]),
		.recv_from_ctrl_mem__msg(fu__recv_from_ctrl_mem__msg[4]),
		.recv_from_ctrl_mem__rdy(fu__recv_from_ctrl_mem__rdy[4]),
		.recv_from_ctrl_mem__val(fu__recv_from_ctrl_mem__val[4]),
		.recv_in__msg(fu__recv_in__msg[4]),
		.recv_in__rdy(fu__recv_in__rdy[4]),
		.recv_in__val(fu__recv_in__val[4]),
		.recv_opt__msg(fu__recv_opt__msg[4]),
		.recv_opt__rdy(fu__recv_opt__rdy[4]),
		.recv_opt__val(fu__recv_opt__val[4]),
		.send_out__msg(fu__send_out__msg[4]),
		.send_out__rdy(fu__send_out__rdy[4]),
		.send_out__val(fu__send_out__val[4]),
		.send_to_ctrl_mem__msg(fu__send_to_ctrl_mem__msg[4]),
		.send_to_ctrl_mem__rdy(fu__send_to_ctrl_mem__rdy[4]),
		.send_to_ctrl_mem__val(fu__send_to_ctrl_mem__val[4]),
		.to_mem_raddr__msg(fu__to_mem_raddr__msg[4]),
		.to_mem_raddr__rdy(fu__to_mem_raddr__rdy[4]),
		.to_mem_raddr__val(fu__to_mem_raddr__val[4]),
		.to_mem_waddr__msg(fu__to_mem_waddr__msg[4]),
		.to_mem_waddr__rdy(fu__to_mem_waddr__rdy[4]),
		.to_mem_waddr__val(fu__to_mem_waddr__val[4]),
		.to_mem_wdata__msg(fu__to_mem_wdata__msg[4]),
		.to_mem_wdata__rdy(fu__to_mem_wdata__rdy[4]),
		.to_mem_wdata__val(fu__to_mem_wdata__val[4])
	);
	CompRTL__00ec249fa9189fd7 fu__5(
		.clear(fu__clear[5]),
		.clk(fu__clk[5]),
		.ctrl_addr_inport(fu__ctrl_addr_inport[5]),
		.reset(fu__reset[5]),
		.from_mem_rdata__msg(fu__from_mem_rdata__msg[5]),
		.from_mem_rdata__rdy(fu__from_mem_rdata__rdy[5]),
		.from_mem_rdata__val(fu__from_mem_rdata__val[5]),
		.recv_const__msg(fu__recv_const__msg[5]),
		.recv_const__rdy(fu__recv_const__rdy[5]),
		.recv_const__val(fu__recv_const__val[5]),
		.recv_from_ctrl_mem__msg(fu__recv_from_ctrl_mem__msg[5]),
		.recv_from_ctrl_mem__rdy(fu__recv_from_ctrl_mem__rdy[5]),
		.recv_from_ctrl_mem__val(fu__recv_from_ctrl_mem__val[5]),
		.recv_in__msg(fu__recv_in__msg[5]),
		.recv_in__rdy(fu__recv_in__rdy[5]),
		.recv_in__val(fu__recv_in__val[5]),
		.recv_opt__msg(fu__recv_opt__msg[5]),
		.recv_opt__rdy(fu__recv_opt__rdy[5]),
		.recv_opt__val(fu__recv_opt__val[5]),
		.send_out__msg(fu__send_out__msg[5]),
		.send_out__rdy(fu__send_out__rdy[5]),
		.send_out__val(fu__send_out__val[5]),
		.send_to_ctrl_mem__msg(fu__send_to_ctrl_mem__msg[5]),
		.send_to_ctrl_mem__rdy(fu__send_to_ctrl_mem__rdy[5]),
		.send_to_ctrl_mem__val(fu__send_to_ctrl_mem__val[5]),
		.to_mem_raddr__msg(fu__to_mem_raddr__msg[5]),
		.to_mem_raddr__rdy(fu__to_mem_raddr__rdy[5]),
		.to_mem_raddr__val(fu__to_mem_raddr__val[5]),
		.to_mem_waddr__msg(fu__to_mem_waddr__msg[5]),
		.to_mem_waddr__rdy(fu__to_mem_waddr__rdy[5]),
		.to_mem_waddr__val(fu__to_mem_waddr__val[5]),
		.to_mem_wdata__msg(fu__to_mem_wdata__msg[5]),
		.to_mem_wdata__rdy(fu__to_mem_wdata__rdy[5]),
		.to_mem_wdata__val(fu__to_mem_wdata__val[5])
	);
	SelRTL__00ec249fa9189fd7 fu__6(
		.clear(fu__clear[6]),
		.clk(fu__clk[6]),
		.ctrl_addr_inport(fu__ctrl_addr_inport[6]),
		.reset(fu__reset[6]),
		.from_mem_rdata__msg(fu__from_mem_rdata__msg[6]),
		.from_mem_rdata__rdy(fu__from_mem_rdata__rdy[6]),
		.from_mem_rdata__val(fu__from_mem_rdata__val[6]),
		.recv_const__msg(fu__recv_const__msg[6]),
		.recv_const__rdy(fu__recv_const__rdy[6]),
		.recv_const__val(fu__recv_const__val[6]),
		.recv_from_ctrl_mem__msg(fu__recv_from_ctrl_mem__msg[6]),
		.recv_from_ctrl_mem__rdy(fu__recv_from_ctrl_mem__rdy[6]),
		.recv_from_ctrl_mem__val(fu__recv_from_ctrl_mem__val[6]),
		.recv_in__msg(fu__recv_in__msg[6]),
		.recv_in__rdy(fu__recv_in__rdy[6]),
		.recv_in__val(fu__recv_in__val[6]),
		.recv_opt__msg(fu__recv_opt__msg[6]),
		.recv_opt__rdy(fu__recv_opt__rdy[6]),
		.recv_opt__val(fu__recv_opt__val[6]),
		.send_out__msg(fu__send_out__msg[6]),
		.send_out__rdy(fu__send_out__rdy[6]),
		.send_out__val(fu__send_out__val[6]),
		.send_to_ctrl_mem__msg(fu__send_to_ctrl_mem__msg[6]),
		.send_to_ctrl_mem__rdy(fu__send_to_ctrl_mem__rdy[6]),
		.send_to_ctrl_mem__val(fu__send_to_ctrl_mem__val[6]),
		.to_mem_raddr__msg(fu__to_mem_raddr__msg[6]),
		.to_mem_raddr__rdy(fu__to_mem_raddr__rdy[6]),
		.to_mem_raddr__val(fu__to_mem_raddr__val[6]),
		.to_mem_waddr__msg(fu__to_mem_waddr__msg[6]),
		.to_mem_waddr__rdy(fu__to_mem_waddr__rdy[6]),
		.to_mem_waddr__val(fu__to_mem_waddr__val[6]),
		.to_mem_wdata__msg(fu__to_mem_wdata__msg[6]),
		.to_mem_wdata__rdy(fu__to_mem_wdata__rdy[6]),
		.to_mem_wdata__val(fu__to_mem_wdata__val[6])
	);
	GrantRTL__00ec249fa9189fd7 fu__7(
		.clear(fu__clear[7]),
		.clk(fu__clk[7]),
		.ctrl_addr_inport(fu__ctrl_addr_inport[7]),
		.reset(fu__reset[7]),
		.from_mem_rdata__msg(fu__from_mem_rdata__msg[7]),
		.from_mem_rdata__rdy(fu__from_mem_rdata__rdy[7]),
		.from_mem_rdata__val(fu__from_mem_rdata__val[7]),
		.recv_const__msg(fu__recv_const__msg[7]),
		.recv_const__rdy(fu__recv_const__rdy[7]),
		.recv_const__val(fu__recv_const__val[7]),
		.recv_from_ctrl_mem__msg(fu__recv_from_ctrl_mem__msg[7]),
		.recv_from_ctrl_mem__rdy(fu__recv_from_ctrl_mem__rdy[7]),
		.recv_from_ctrl_mem__val(fu__recv_from_ctrl_mem__val[7]),
		.recv_in__msg(fu__recv_in__msg[7]),
		.recv_in__rdy(fu__recv_in__rdy[7]),
		.recv_in__val(fu__recv_in__val[7]),
		.recv_opt__msg(fu__recv_opt__msg[7]),
		.recv_opt__rdy(fu__recv_opt__rdy[7]),
		.recv_opt__val(fu__recv_opt__val[7]),
		.send_out__msg(fu__send_out__msg[7]),
		.send_out__rdy(fu__send_out__rdy[7]),
		.send_out__val(fu__send_out__val[7]),
		.send_to_ctrl_mem__msg(fu__send_to_ctrl_mem__msg[7]),
		.send_to_ctrl_mem__rdy(fu__send_to_ctrl_mem__rdy[7]),
		.send_to_ctrl_mem__val(fu__send_to_ctrl_mem__val[7]),
		.to_mem_raddr__msg(fu__to_mem_raddr__msg[7]),
		.to_mem_raddr__rdy(fu__to_mem_raddr__rdy[7]),
		.to_mem_raddr__val(fu__to_mem_raddr__val[7]),
		.to_mem_waddr__msg(fu__to_mem_waddr__msg[7]),
		.to_mem_waddr__rdy(fu__to_mem_waddr__rdy[7]),
		.to_mem_waddr__val(fu__to_mem_waddr__val[7]),
		.to_mem_wdata__msg(fu__to_mem_wdata__msg[7]),
		.to_mem_wdata__rdy(fu__to_mem_wdata__rdy[7]),
		.to_mem_wdata__val(fu__to_mem_wdata__val[7])
	);
	PhiRTL__00ec249fa9189fd7 fu__8(
		.clear(fu__clear[8]),
		.clk(fu__clk[8]),
		.ctrl_addr_inport(fu__ctrl_addr_inport[8]),
		.reset(fu__reset[8]),
		.from_mem_rdata__msg(fu__from_mem_rdata__msg[8]),
		.from_mem_rdata__rdy(fu__from_mem_rdata__rdy[8]),
		.from_mem_rdata__val(fu__from_mem_rdata__val[8]),
		.recv_const__msg(fu__recv_const__msg[8]),
		.recv_const__rdy(fu__recv_const__rdy[8]),
		.recv_const__val(fu__recv_const__val[8]),
		.recv_from_ctrl_mem__msg(fu__recv_from_ctrl_mem__msg[8]),
		.recv_from_ctrl_mem__rdy(fu__recv_from_ctrl_mem__rdy[8]),
		.recv_from_ctrl_mem__val(fu__recv_from_ctrl_mem__val[8]),
		.recv_in__msg(fu__recv_in__msg[8]),
		.recv_in__rdy(fu__recv_in__rdy[8]),
		.recv_in__val(fu__recv_in__val[8]),
		.recv_opt__msg(fu__recv_opt__msg[8]),
		.recv_opt__rdy(fu__recv_opt__rdy[8]),
		.recv_opt__val(fu__recv_opt__val[8]),
		.send_out__msg(fu__send_out__msg[8]),
		.send_out__rdy(fu__send_out__rdy[8]),
		.send_out__val(fu__send_out__val[8]),
		.send_to_ctrl_mem__msg(fu__send_to_ctrl_mem__msg[8]),
		.send_to_ctrl_mem__rdy(fu__send_to_ctrl_mem__rdy[8]),
		.send_to_ctrl_mem__val(fu__send_to_ctrl_mem__val[8]),
		.to_mem_raddr__msg(fu__to_mem_raddr__msg[8]),
		.to_mem_raddr__rdy(fu__to_mem_raddr__rdy[8]),
		.to_mem_raddr__val(fu__to_mem_raddr__val[8]),
		.to_mem_waddr__msg(fu__to_mem_waddr__msg[8]),
		.to_mem_waddr__rdy(fu__to_mem_waddr__rdy[8]),
		.to_mem_waddr__val(fu__to_mem_waddr__val[8]),
		.to_mem_wdata__msg(fu__to_mem_wdata__msg[8]),
		.to_mem_wdata__rdy(fu__to_mem_wdata__rdy[8]),
		.to_mem_wdata__val(fu__to_mem_wdata__val[8])
	);
	MemUnitRTL__00ec249fa9189fd7 fu__9(
		.clear(fu__clear[9]),
		.clk(fu__clk[9]),
		.ctrl_addr_inport(fu__ctrl_addr_inport[9]),
		.reset(fu__reset[9]),
		.from_mem_rdata__msg(fu__from_mem_rdata__msg[9]),
		.from_mem_rdata__rdy(fu__from_mem_rdata__rdy[9]),
		.from_mem_rdata__val(fu__from_mem_rdata__val[9]),
		.recv_const__msg(fu__recv_const__msg[9]),
		.recv_const__rdy(fu__recv_const__rdy[9]),
		.recv_const__val(fu__recv_const__val[9]),
		.recv_from_ctrl_mem__msg(fu__recv_from_ctrl_mem__msg[9]),
		.recv_from_ctrl_mem__rdy(fu__recv_from_ctrl_mem__rdy[9]),
		.recv_from_ctrl_mem__val(fu__recv_from_ctrl_mem__val[9]),
		.recv_in__msg(fu__recv_in__msg[9]),
		.recv_in__rdy(fu__recv_in__rdy[9]),
		.recv_in__val(fu__recv_in__val[9]),
		.recv_opt__msg(fu__recv_opt__msg[9]),
		.recv_opt__rdy(fu__recv_opt__rdy[9]),
		.recv_opt__val(fu__recv_opt__val[9]),
		.send_out__msg(fu__send_out__msg[9]),
		.send_out__rdy(fu__send_out__rdy[9]),
		.send_out__val(fu__send_out__val[9]),
		.send_to_ctrl_mem__msg(fu__send_to_ctrl_mem__msg[9]),
		.send_to_ctrl_mem__rdy(fu__send_to_ctrl_mem__rdy[9]),
		.send_to_ctrl_mem__val(fu__send_to_ctrl_mem__val[9]),
		.to_mem_raddr__msg(fu__to_mem_raddr__msg[9]),
		.to_mem_raddr__rdy(fu__to_mem_raddr__rdy[9]),
		.to_mem_raddr__val(fu__to_mem_raddr__val[9]),
		.to_mem_waddr__msg(fu__to_mem_waddr__msg[9]),
		.to_mem_waddr__rdy(fu__to_mem_waddr__rdy[9]),
		.to_mem_waddr__val(fu__to_mem_waddr__val[9]),
		.to_mem_wdata__msg(fu__to_mem_wdata__msg[9]),
		.to_mem_wdata__rdy(fu__to_mem_wdata__rdy[9]),
		.to_mem_wdata__val(fu__to_mem_wdata__val[9])
	);
	RetRTL__00ec249fa9189fd7 fu__10(
		.clear(fu__clear[10]),
		.clk(fu__clk[10]),
		.ctrl_addr_inport(fu__ctrl_addr_inport[10]),
		.reset(fu__reset[10]),
		.from_mem_rdata__msg(fu__from_mem_rdata__msg[10]),
		.from_mem_rdata__rdy(fu__from_mem_rdata__rdy[10]),
		.from_mem_rdata__val(fu__from_mem_rdata__val[10]),
		.recv_const__msg(fu__recv_const__msg[10]),
		.recv_const__rdy(fu__recv_const__rdy[10]),
		.recv_const__val(fu__recv_const__val[10]),
		.recv_from_ctrl_mem__msg(fu__recv_from_ctrl_mem__msg[10]),
		.recv_from_ctrl_mem__rdy(fu__recv_from_ctrl_mem__rdy[10]),
		.recv_from_ctrl_mem__val(fu__recv_from_ctrl_mem__val[10]),
		.recv_in__msg(fu__recv_in__msg[10]),
		.recv_in__rdy(fu__recv_in__rdy[10]),
		.recv_in__val(fu__recv_in__val[10]),
		.recv_opt__msg(fu__recv_opt__msg[10]),
		.recv_opt__rdy(fu__recv_opt__rdy[10]),
		.recv_opt__val(fu__recv_opt__val[10]),
		.send_out__msg(fu__send_out__msg[10]),
		.send_out__rdy(fu__send_out__rdy[10]),
		.send_out__val(fu__send_out__val[10]),
		.send_to_ctrl_mem__msg(fu__send_to_ctrl_mem__msg[10]),
		.send_to_ctrl_mem__rdy(fu__send_to_ctrl_mem__rdy[10]),
		.send_to_ctrl_mem__val(fu__send_to_ctrl_mem__val[10]),
		.to_mem_raddr__msg(fu__to_mem_raddr__msg[10]),
		.to_mem_raddr__rdy(fu__to_mem_raddr__rdy[10]),
		.to_mem_raddr__val(fu__to_mem_raddr__val[10]),
		.to_mem_waddr__msg(fu__to_mem_waddr__msg[10]),
		.to_mem_waddr__rdy(fu__to_mem_waddr__rdy[10]),
		.to_mem_waddr__val(fu__to_mem_waddr__val[10]),
		.to_mem_wdata__msg(fu__to_mem_wdata__msg[10]),
		.to_mem_wdata__rdy(fu__to_mem_wdata__rdy[10]),
		.to_mem_wdata__val(fu__to_mem_wdata__val[10])
	);
	GepRTL__00ec249fa9189fd7 fu__11(
		.clear(fu__clear[11]),
		.clk(fu__clk[11]),
		.ctrl_addr_inport(fu__ctrl_addr_inport[11]),
		.reset(fu__reset[11]),
		.from_mem_rdata__msg(fu__from_mem_rdata__msg[11]),
		.from_mem_rdata__rdy(fu__from_mem_rdata__rdy[11]),
		.from_mem_rdata__val(fu__from_mem_rdata__val[11]),
		.recv_const__msg(fu__recv_const__msg[11]),
		.recv_const__rdy(fu__recv_const__rdy[11]),
		.recv_const__val(fu__recv_const__val[11]),
		.recv_from_ctrl_mem__msg(fu__recv_from_ctrl_mem__msg[11]),
		.recv_from_ctrl_mem__rdy(fu__recv_from_ctrl_mem__rdy[11]),
		.recv_from_ctrl_mem__val(fu__recv_from_ctrl_mem__val[11]),
		.recv_in__msg(fu__recv_in__msg[11]),
		.recv_in__rdy(fu__recv_in__rdy[11]),
		.recv_in__val(fu__recv_in__val[11]),
		.recv_opt__msg(fu__recv_opt__msg[11]),
		.recv_opt__rdy(fu__recv_opt__rdy[11]),
		.recv_opt__val(fu__recv_opt__val[11]),
		.send_out__msg(fu__send_out__msg[11]),
		.send_out__rdy(fu__send_out__rdy[11]),
		.send_out__val(fu__send_out__val[11]),
		.send_to_ctrl_mem__msg(fu__send_to_ctrl_mem__msg[11]),
		.send_to_ctrl_mem__rdy(fu__send_to_ctrl_mem__rdy[11]),
		.send_to_ctrl_mem__val(fu__send_to_ctrl_mem__val[11]),
		.to_mem_raddr__msg(fu__to_mem_raddr__msg[11]),
		.to_mem_raddr__rdy(fu__to_mem_raddr__rdy[11]),
		.to_mem_raddr__val(fu__to_mem_raddr__val[11]),
		.to_mem_waddr__msg(fu__to_mem_waddr__msg[11]),
		.to_mem_waddr__rdy(fu__to_mem_waddr__rdy[11]),
		.to_mem_waddr__val(fu__to_mem_waddr__val[11]),
		.to_mem_wdata__msg(fu__to_mem_wdata__msg[11]),
		.to_mem_wdata__rdy(fu__to_mem_wdata__rdy[11]),
		.to_mem_wdata__val(fu__to_mem_wdata__val[11])
	);
	NahRTL__00ec249fa9189fd7 fu__12(
		.clear(fu__clear[12]),
		.clk(fu__clk[12]),
		.ctrl_addr_inport(fu__ctrl_addr_inport[12]),
		.reset(fu__reset[12]),
		.from_mem_rdata__msg(fu__from_mem_rdata__msg[12]),
		.from_mem_rdata__rdy(fu__from_mem_rdata__rdy[12]),
		.from_mem_rdata__val(fu__from_mem_rdata__val[12]),
		.recv_const__msg(fu__recv_const__msg[12]),
		.recv_const__rdy(fu__recv_const__rdy[12]),
		.recv_const__val(fu__recv_const__val[12]),
		.recv_from_ctrl_mem__msg(fu__recv_from_ctrl_mem__msg[12]),
		.recv_from_ctrl_mem__rdy(fu__recv_from_ctrl_mem__rdy[12]),
		.recv_from_ctrl_mem__val(fu__recv_from_ctrl_mem__val[12]),
		.recv_in__msg(fu__recv_in__msg[12]),
		.recv_in__rdy(fu__recv_in__rdy[12]),
		.recv_in__val(fu__recv_in__val[12]),
		.recv_opt__msg(fu__recv_opt__msg[12]),
		.recv_opt__rdy(fu__recv_opt__rdy[12]),
		.recv_opt__val(fu__recv_opt__val[12]),
		.send_out__msg(fu__send_out__msg[12]),
		.send_out__rdy(fu__send_out__rdy[12]),
		.send_out__val(fu__send_out__val[12]),
		.send_to_ctrl_mem__msg(fu__send_to_ctrl_mem__msg[12]),
		.send_to_ctrl_mem__rdy(fu__send_to_ctrl_mem__rdy[12]),
		.send_to_ctrl_mem__val(fu__send_to_ctrl_mem__val[12]),
		.to_mem_raddr__msg(fu__to_mem_raddr__msg[12]),
		.to_mem_raddr__rdy(fu__to_mem_raddr__rdy[12]),
		.to_mem_raddr__val(fu__to_mem_raddr__val[12]),
		.to_mem_waddr__msg(fu__to_mem_waddr__msg[12]),
		.to_mem_waddr__rdy(fu__to_mem_waddr__rdy[12]),
		.to_mem_waddr__val(fu__to_mem_waddr__val[12]),
		.to_mem_wdata__msg(fu__to_mem_wdata__msg[12]),
		.to_mem_wdata__rdy(fu__to_mem_wdata__rdy[12]),
		.to_mem_wdata__val(fu__to_mem_wdata__val[12])
	);
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	function automatic [3:0] sv2v_cast_4;
		input reg [3:0] inp;
		sv2v_cast_4 = inp;
	endfunction
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	always @(*) begin : comb_logic
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] j;
			for (j = 1'd0; j < __const__num_outports_at_comb_logic; j = j + 1'd1)
				begin
					send_out__val[1 - sv2v_cast_1(j)+:1] = 1'd0;
					send_out__msg[(1 - sv2v_cast_1(j)) * 35+:35] = 35'h000000000;
				end
		end
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < 4'd13; i = i + 1'd1)
				begin
					fu__recv_const__msg[sv2v_cast_4(i)] = recv_const__msg;
					fu__recv_const__val[sv2v_cast_4(i)] = recv_const__val;
					fu_recv_const_rdy_vector[sv2v_cast_4(i)] = fu__recv_const__rdy[sv2v_cast_4(i)];
					fu__recv_opt__msg[sv2v_cast_4(i)] = recv_opt__msg;
					if (prologue_count_inport != 3'd0)
						fu__recv_opt__msg[sv2v_cast_4(i)][110-:7] = __const__OPT_NAH;
					fu__recv_opt__val[sv2v_cast_4(i)] = recv_opt__val;
					fu_recv_opt_rdy_vector[sv2v_cast_4(i)] = fu__recv_opt__rdy[sv2v_cast_4(i)];
					begin : sv2v_autoblock_3
						reg [31:0] j;
						for (j = 1'd0; j < __const__num_outports_at_comb_logic; j = j + 1'd1)
							begin
								if (fu__send_out__val[sv2v_cast_4(i)][1 - sv2v_cast_1(j)+:1]) begin
									send_out__msg[(1 - sv2v_cast_1(j)) * 35+:35] = fu__send_out__msg[sv2v_cast_4(i)][(1 - sv2v_cast_1(j)) * 35+:35];
									send_out__val[1 - sv2v_cast_1(j)+:1] = fu__send_out__val[sv2v_cast_4(i)][1 - sv2v_cast_1(j)+:1];
								end
								fu__send_out__rdy[sv2v_cast_4(i)][1 - sv2v_cast_1(j)+:1] = send_out__rdy[1 - sv2v_cast_1(j)+:1];
							end
					end
				end
		end
		recv_const__rdy = |fu_recv_const_rdy_vector;
		recv_opt__rdy = |fu_recv_opt_rdy_vector | (prologue_count_inport != 3'd0);
		begin : sv2v_autoblock_4
			reg [31:0] j;
			for (j = 1'd0; j < __const__num_inports_at_comb_logic; j = j + 1'd1)
				recv_in__rdy[3 - sv2v_cast_2(j)+:1] = 1'd0;
		end
		begin : sv2v_autoblock_5
			reg [31:0] port;
			for (port = 1'd0; port < __const__num_inports_at_comb_logic; port = port + 1'd1)
				begin
					begin : sv2v_autoblock_6
						reg [31:0] i;
						for (i = 1'd0; i < 4'd13; i = i + 1'd1)
							begin
								fu__recv_in__msg[sv2v_cast_4(i)][(3 - sv2v_cast_2(port)) * 35+:35] = recv_in__msg[(3 - sv2v_cast_2(port)) * 35+:35];
								fu__recv_in__val[sv2v_cast_4(i)][3 - sv2v_cast_2(port)+:1] = recv_in__val[3 - sv2v_cast_2(port)+:1];
								fu_recv_in_rdy_vector[sv2v_cast_2(port)][sv2v_cast_4(i)] = fu__recv_in__rdy[sv2v_cast_4(i)][3 - sv2v_cast_2(port)+:1];
							end
					end
					recv_in__rdy[3 - sv2v_cast_2(port)+:1] = |fu_recv_in_rdy_vector[sv2v_cast_2(port)];
				end
		end
	end
	always @(*) begin : connect_to_controller
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_7
			reg [31:0] i;
			for (i = 1'd0; i < 4'd13; i = i + 1'd1)
				begin
					fu__recv_from_ctrl_mem__msg[sv2v_cast_4(i)] = recv_from_ctrl_mem__msg;
					fu__recv_from_ctrl_mem__val[sv2v_cast_4(i)] = recv_from_ctrl_mem__val;
					recv_from_controller_rdy_vector[sv2v_cast_4(i)] = fu__recv_from_ctrl_mem__rdy[sv2v_cast_4(i)];
				end
		end
		recv_from_ctrl_mem__rdy = |recv_from_controller_rdy_vector;
		send_to_ctrl_mem__msg = 173'h00000000000000000000000000000000000000000000;
		send_to_ctrl_mem__val = 1'd0;
		begin : sv2v_autoblock_8
			reg [31:0] i;
			for (i = 1'd0; i < 4'd13; i = i + 1'd1)
				begin
					if (fu__send_to_ctrl_mem__val[sv2v_cast_4(i)]) begin
						send_to_ctrl_mem__msg = fu__send_to_ctrl_mem__msg[sv2v_cast_4(i)];
						send_to_ctrl_mem__val = fu__send_to_ctrl_mem__val[sv2v_cast_4(i)];
					end
					fu__send_to_ctrl_mem__rdy[sv2v_cast_4(i)] = send_to_ctrl_mem__rdy;
					fu__ctrl_addr_inport[sv2v_cast_4(i)] = ctrl_addr_inport;
				end
		end
	end
	assign fu__clk[0] = clk;
	assign fu__reset[0] = reset;
	assign fu__clk[1] = clk;
	assign fu__reset[1] = reset;
	assign fu__clk[2] = clk;
	assign fu__reset[2] = reset;
	assign fu__clk[3] = clk;
	assign fu__reset[3] = reset;
	assign fu__clk[4] = clk;
	assign fu__reset[4] = reset;
	assign fu__clk[5] = clk;
	assign fu__reset[5] = reset;
	assign fu__clk[6] = clk;
	assign fu__reset[6] = reset;
	assign fu__clk[7] = clk;
	assign fu__reset[7] = reset;
	assign fu__clk[8] = clk;
	assign fu__reset[8] = reset;
	assign fu__clk[9] = clk;
	assign fu__reset[9] = reset;
	assign fu__clk[10] = clk;
	assign fu__reset[10] = reset;
	assign fu__clk[11] = clk;
	assign fu__reset[11] = reset;
	assign fu__clk[12] = clk;
	assign fu__reset[12] = reset;
	assign to_mem_raddr__msg[180+:15] = fu__to_mem_raddr__msg[0];
	assign fu__to_mem_raddr__rdy[0] = to_mem_raddr__rdy[12+:1];
	assign to_mem_raddr__val[12+:1] = fu__to_mem_raddr__val[0];
	assign fu__from_mem_rdata__msg[0] = from_mem_rdata__msg[420+:35];
	assign from_mem_rdata__rdy[12+:1] = fu__from_mem_rdata__rdy[0];
	assign fu__from_mem_rdata__val[0] = from_mem_rdata__val[12+:1];
	assign to_mem_waddr__msg[180+:15] = fu__to_mem_waddr__msg[0];
	assign fu__to_mem_waddr__rdy[0] = to_mem_waddr__rdy[12+:1];
	assign to_mem_waddr__val[12+:1] = fu__to_mem_waddr__val[0];
	assign to_mem_wdata__msg[420+:35] = fu__to_mem_wdata__msg[0];
	assign fu__to_mem_wdata__rdy[0] = to_mem_wdata__rdy[12+:1];
	assign to_mem_wdata__val[12+:1] = fu__to_mem_wdata__val[0];
	assign fu__clear[0] = clear[12+:1];
	assign to_mem_raddr__msg[165+:15] = fu__to_mem_raddr__msg[1];
	assign fu__to_mem_raddr__rdy[1] = to_mem_raddr__rdy[11+:1];
	assign to_mem_raddr__val[11+:1] = fu__to_mem_raddr__val[1];
	assign fu__from_mem_rdata__msg[1] = from_mem_rdata__msg[385+:35];
	assign from_mem_rdata__rdy[11+:1] = fu__from_mem_rdata__rdy[1];
	assign fu__from_mem_rdata__val[1] = from_mem_rdata__val[11+:1];
	assign to_mem_waddr__msg[165+:15] = fu__to_mem_waddr__msg[1];
	assign fu__to_mem_waddr__rdy[1] = to_mem_waddr__rdy[11+:1];
	assign to_mem_waddr__val[11+:1] = fu__to_mem_waddr__val[1];
	assign to_mem_wdata__msg[385+:35] = fu__to_mem_wdata__msg[1];
	assign fu__to_mem_wdata__rdy[1] = to_mem_wdata__rdy[11+:1];
	assign to_mem_wdata__val[11+:1] = fu__to_mem_wdata__val[1];
	assign fu__clear[1] = clear[11+:1];
	assign to_mem_raddr__msg[150+:15] = fu__to_mem_raddr__msg[2];
	assign fu__to_mem_raddr__rdy[2] = to_mem_raddr__rdy[10+:1];
	assign to_mem_raddr__val[10+:1] = fu__to_mem_raddr__val[2];
	assign fu__from_mem_rdata__msg[2] = from_mem_rdata__msg[350+:35];
	assign from_mem_rdata__rdy[10+:1] = fu__from_mem_rdata__rdy[2];
	assign fu__from_mem_rdata__val[2] = from_mem_rdata__val[10+:1];
	assign to_mem_waddr__msg[150+:15] = fu__to_mem_waddr__msg[2];
	assign fu__to_mem_waddr__rdy[2] = to_mem_waddr__rdy[10+:1];
	assign to_mem_waddr__val[10+:1] = fu__to_mem_waddr__val[2];
	assign to_mem_wdata__msg[350+:35] = fu__to_mem_wdata__msg[2];
	assign fu__to_mem_wdata__rdy[2] = to_mem_wdata__rdy[10+:1];
	assign to_mem_wdata__val[10+:1] = fu__to_mem_wdata__val[2];
	assign fu__clear[2] = clear[10+:1];
	assign to_mem_raddr__msg[135+:15] = fu__to_mem_raddr__msg[3];
	assign fu__to_mem_raddr__rdy[3] = to_mem_raddr__rdy[9+:1];
	assign to_mem_raddr__val[9+:1] = fu__to_mem_raddr__val[3];
	assign fu__from_mem_rdata__msg[3] = from_mem_rdata__msg[315+:35];
	assign from_mem_rdata__rdy[9+:1] = fu__from_mem_rdata__rdy[3];
	assign fu__from_mem_rdata__val[3] = from_mem_rdata__val[9+:1];
	assign to_mem_waddr__msg[135+:15] = fu__to_mem_waddr__msg[3];
	assign fu__to_mem_waddr__rdy[3] = to_mem_waddr__rdy[9+:1];
	assign to_mem_waddr__val[9+:1] = fu__to_mem_waddr__val[3];
	assign to_mem_wdata__msg[315+:35] = fu__to_mem_wdata__msg[3];
	assign fu__to_mem_wdata__rdy[3] = to_mem_wdata__rdy[9+:1];
	assign to_mem_wdata__val[9+:1] = fu__to_mem_wdata__val[3];
	assign fu__clear[3] = clear[9+:1];
	assign to_mem_raddr__msg[120+:15] = fu__to_mem_raddr__msg[4];
	assign fu__to_mem_raddr__rdy[4] = to_mem_raddr__rdy[8+:1];
	assign to_mem_raddr__val[8+:1] = fu__to_mem_raddr__val[4];
	assign fu__from_mem_rdata__msg[4] = from_mem_rdata__msg[280+:35];
	assign from_mem_rdata__rdy[8+:1] = fu__from_mem_rdata__rdy[4];
	assign fu__from_mem_rdata__val[4] = from_mem_rdata__val[8+:1];
	assign to_mem_waddr__msg[120+:15] = fu__to_mem_waddr__msg[4];
	assign fu__to_mem_waddr__rdy[4] = to_mem_waddr__rdy[8+:1];
	assign to_mem_waddr__val[8+:1] = fu__to_mem_waddr__val[4];
	assign to_mem_wdata__msg[280+:35] = fu__to_mem_wdata__msg[4];
	assign fu__to_mem_wdata__rdy[4] = to_mem_wdata__rdy[8+:1];
	assign to_mem_wdata__val[8+:1] = fu__to_mem_wdata__val[4];
	assign fu__clear[4] = clear[8+:1];
	assign to_mem_raddr__msg[105+:15] = fu__to_mem_raddr__msg[5];
	assign fu__to_mem_raddr__rdy[5] = to_mem_raddr__rdy[7+:1];
	assign to_mem_raddr__val[7+:1] = fu__to_mem_raddr__val[5];
	assign fu__from_mem_rdata__msg[5] = from_mem_rdata__msg[245+:35];
	assign from_mem_rdata__rdy[7+:1] = fu__from_mem_rdata__rdy[5];
	assign fu__from_mem_rdata__val[5] = from_mem_rdata__val[7+:1];
	assign to_mem_waddr__msg[105+:15] = fu__to_mem_waddr__msg[5];
	assign fu__to_mem_waddr__rdy[5] = to_mem_waddr__rdy[7+:1];
	assign to_mem_waddr__val[7+:1] = fu__to_mem_waddr__val[5];
	assign to_mem_wdata__msg[245+:35] = fu__to_mem_wdata__msg[5];
	assign fu__to_mem_wdata__rdy[5] = to_mem_wdata__rdy[7+:1];
	assign to_mem_wdata__val[7+:1] = fu__to_mem_wdata__val[5];
	assign fu__clear[5] = clear[7+:1];
	assign to_mem_raddr__msg[90+:15] = fu__to_mem_raddr__msg[6];
	assign fu__to_mem_raddr__rdy[6] = to_mem_raddr__rdy[6+:1];
	assign to_mem_raddr__val[6+:1] = fu__to_mem_raddr__val[6];
	assign fu__from_mem_rdata__msg[6] = from_mem_rdata__msg[210+:35];
	assign from_mem_rdata__rdy[6+:1] = fu__from_mem_rdata__rdy[6];
	assign fu__from_mem_rdata__val[6] = from_mem_rdata__val[6+:1];
	assign to_mem_waddr__msg[90+:15] = fu__to_mem_waddr__msg[6];
	assign fu__to_mem_waddr__rdy[6] = to_mem_waddr__rdy[6+:1];
	assign to_mem_waddr__val[6+:1] = fu__to_mem_waddr__val[6];
	assign to_mem_wdata__msg[210+:35] = fu__to_mem_wdata__msg[6];
	assign fu__to_mem_wdata__rdy[6] = to_mem_wdata__rdy[6+:1];
	assign to_mem_wdata__val[6+:1] = fu__to_mem_wdata__val[6];
	assign fu__clear[6] = clear[6+:1];
	assign to_mem_raddr__msg[75+:15] = fu__to_mem_raddr__msg[7];
	assign fu__to_mem_raddr__rdy[7] = to_mem_raddr__rdy[5+:1];
	assign to_mem_raddr__val[5+:1] = fu__to_mem_raddr__val[7];
	assign fu__from_mem_rdata__msg[7] = from_mem_rdata__msg[175+:35];
	assign from_mem_rdata__rdy[5+:1] = fu__from_mem_rdata__rdy[7];
	assign fu__from_mem_rdata__val[7] = from_mem_rdata__val[5+:1];
	assign to_mem_waddr__msg[75+:15] = fu__to_mem_waddr__msg[7];
	assign fu__to_mem_waddr__rdy[7] = to_mem_waddr__rdy[5+:1];
	assign to_mem_waddr__val[5+:1] = fu__to_mem_waddr__val[7];
	assign to_mem_wdata__msg[175+:35] = fu__to_mem_wdata__msg[7];
	assign fu__to_mem_wdata__rdy[7] = to_mem_wdata__rdy[5+:1];
	assign to_mem_wdata__val[5+:1] = fu__to_mem_wdata__val[7];
	assign fu__clear[7] = clear[5+:1];
	assign to_mem_raddr__msg[60+:15] = fu__to_mem_raddr__msg[8];
	assign fu__to_mem_raddr__rdy[8] = to_mem_raddr__rdy[4+:1];
	assign to_mem_raddr__val[4+:1] = fu__to_mem_raddr__val[8];
	assign fu__from_mem_rdata__msg[8] = from_mem_rdata__msg[140+:35];
	assign from_mem_rdata__rdy[4+:1] = fu__from_mem_rdata__rdy[8];
	assign fu__from_mem_rdata__val[8] = from_mem_rdata__val[4+:1];
	assign to_mem_waddr__msg[60+:15] = fu__to_mem_waddr__msg[8];
	assign fu__to_mem_waddr__rdy[8] = to_mem_waddr__rdy[4+:1];
	assign to_mem_waddr__val[4+:1] = fu__to_mem_waddr__val[8];
	assign to_mem_wdata__msg[140+:35] = fu__to_mem_wdata__msg[8];
	assign fu__to_mem_wdata__rdy[8] = to_mem_wdata__rdy[4+:1];
	assign to_mem_wdata__val[4+:1] = fu__to_mem_wdata__val[8];
	assign fu__clear[8] = clear[4+:1];
	assign to_mem_raddr__msg[45+:15] = fu__to_mem_raddr__msg[9];
	assign fu__to_mem_raddr__rdy[9] = to_mem_raddr__rdy[3+:1];
	assign to_mem_raddr__val[3+:1] = fu__to_mem_raddr__val[9];
	assign fu__from_mem_rdata__msg[9] = from_mem_rdata__msg[105+:35];
	assign from_mem_rdata__rdy[3+:1] = fu__from_mem_rdata__rdy[9];
	assign fu__from_mem_rdata__val[9] = from_mem_rdata__val[3+:1];
	assign to_mem_waddr__msg[45+:15] = fu__to_mem_waddr__msg[9];
	assign fu__to_mem_waddr__rdy[9] = to_mem_waddr__rdy[3+:1];
	assign to_mem_waddr__val[3+:1] = fu__to_mem_waddr__val[9];
	assign to_mem_wdata__msg[105+:35] = fu__to_mem_wdata__msg[9];
	assign fu__to_mem_wdata__rdy[9] = to_mem_wdata__rdy[3+:1];
	assign to_mem_wdata__val[3+:1] = fu__to_mem_wdata__val[9];
	assign fu__clear[9] = clear[3+:1];
	assign to_mem_raddr__msg[30+:15] = fu__to_mem_raddr__msg[10];
	assign fu__to_mem_raddr__rdy[10] = to_mem_raddr__rdy[2+:1];
	assign to_mem_raddr__val[2+:1] = fu__to_mem_raddr__val[10];
	assign fu__from_mem_rdata__msg[10] = from_mem_rdata__msg[70+:35];
	assign from_mem_rdata__rdy[2+:1] = fu__from_mem_rdata__rdy[10];
	assign fu__from_mem_rdata__val[10] = from_mem_rdata__val[2+:1];
	assign to_mem_waddr__msg[30+:15] = fu__to_mem_waddr__msg[10];
	assign fu__to_mem_waddr__rdy[10] = to_mem_waddr__rdy[2+:1];
	assign to_mem_waddr__val[2+:1] = fu__to_mem_waddr__val[10];
	assign to_mem_wdata__msg[70+:35] = fu__to_mem_wdata__msg[10];
	assign fu__to_mem_wdata__rdy[10] = to_mem_wdata__rdy[2+:1];
	assign to_mem_wdata__val[2+:1] = fu__to_mem_wdata__val[10];
	assign fu__clear[10] = clear[2+:1];
	assign to_mem_raddr__msg[15+:15] = fu__to_mem_raddr__msg[11];
	assign fu__to_mem_raddr__rdy[11] = to_mem_raddr__rdy[1+:1];
	assign to_mem_raddr__val[1+:1] = fu__to_mem_raddr__val[11];
	assign fu__from_mem_rdata__msg[11] = from_mem_rdata__msg[35+:35];
	assign from_mem_rdata__rdy[1+:1] = fu__from_mem_rdata__rdy[11];
	assign fu__from_mem_rdata__val[11] = from_mem_rdata__val[1+:1];
	assign to_mem_waddr__msg[15+:15] = fu__to_mem_waddr__msg[11];
	assign fu__to_mem_waddr__rdy[11] = to_mem_waddr__rdy[1+:1];
	assign to_mem_waddr__val[1+:1] = fu__to_mem_waddr__val[11];
	assign to_mem_wdata__msg[35+:35] = fu__to_mem_wdata__msg[11];
	assign fu__to_mem_wdata__rdy[11] = to_mem_wdata__rdy[1+:1];
	assign to_mem_wdata__val[1+:1] = fu__to_mem_wdata__val[11];
	assign fu__clear[11] = clear[1+:1];
	assign to_mem_raddr__msg[0+:15] = fu__to_mem_raddr__msg[12];
	assign fu__to_mem_raddr__rdy[12] = to_mem_raddr__rdy[0+:1];
	assign to_mem_raddr__val[0+:1] = fu__to_mem_raddr__val[12];
	assign fu__from_mem_rdata__msg[12] = from_mem_rdata__msg[0+:35];
	assign from_mem_rdata__rdy[0+:1] = fu__from_mem_rdata__rdy[12];
	assign fu__from_mem_rdata__val[12] = from_mem_rdata__val[0+:1];
	assign to_mem_waddr__msg[0+:15] = fu__to_mem_waddr__msg[12];
	assign fu__to_mem_waddr__rdy[12] = to_mem_waddr__rdy[0+:1];
	assign to_mem_waddr__val[0+:1] = fu__to_mem_waddr__val[12];
	assign to_mem_wdata__msg[0+:35] = fu__to_mem_wdata__msg[12];
	assign fu__to_mem_wdata__rdy[12] = to_mem_wdata__rdy[0+:1];
	assign to_mem_wdata__val[0+:1] = fu__to_mem_wdata__val[12];
	assign fu__clear[12] = clear[0+:1];
	initial _sv2v_0 = 0;
endmodule
module CrossbarRTL__af5511e92d2ad43c (
	cgra_id,
	clear,
	clk,
	compute_done,
	crossbar_id,
	crossbar_outport,
	ctrl_addr_inport,
	prologue_count_inport,
	reset,
	tile_id,
	recv_data__msg,
	recv_data__rdy,
	recv_data__val,
	recv_opt__msg,
	recv_opt__rdy,
	recv_opt__val,
	send_data__msg,
	send_data__rdy,
	send_data__val
);
	reg _sv2v_0;
	input wire [0:0] cgra_id;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [0:0] compute_done;
	input wire [0:0] crossbar_id;
	input wire [15:0] crossbar_outport;
	input wire [5:0] ctrl_addr_inport;
	input wire [287:0] prologue_count_inport;
	input wire [0:0] reset;
	input wire [4:0] tile_id;
	input wire [69:0] recv_data__msg;
	output reg [1:0] recv_data__rdy;
	input wire [1:0] recv_data__val;
	input wire [110:0] recv_opt__msg;
	output reg [0:0] recv_opt__rdy;
	input wire [0:0] recv_opt__val;
	output reg [279:0] send_data__msg;
	input wire [7:0] send_data__rdy;
	output reg [7:0] send_data__val;
	localparam [1:0] __const__num_inports_at_update_signal = 2'd2;
	localparam [3:0] __const__num_outports_at_update_signal = 4'd8;
	localparam [6:0] __const__OPT_START = 7'd0;
	localparam [5:0] __const__ctrl_mem_size_at_update_prologue_counter = 6'd48;
	localparam [1:0] __const__num_inports_at_update_prologue_counter = 2'd2;
	localparam [5:0] __const__ctrl_mem_size_at_update_prologue_counter_next = 6'd48;
	localparam [1:0] __const__num_inports_at_update_prologue_counter_next = 2'd2;
	localparam [3:0] __const__num_outports_at_update_prologue_counter_next = 4'd8;
	localparam [3:0] __const__num_outports_at_update_send_accepted_next = 4'd8;
	localparam [3:0] __const__num_outports_at_update_prologue_allowing_vector = 4'd8;
	localparam [3:0] __const__num_outports_at_update_prologue_or_valid_vector = 4'd8;
	localparam [3:0] __const__num_outports_at_update_in_dir_vector = 4'd8;
	localparam [3:0] __const__num_outports_at_update_rdy_vector = 4'd8;
	localparam [2:0] __const__outport_towards_local_base_id_at_update_rdy_vector = 3'd4;
	localparam [3:0] __const__num_outports_at_update_valid_vector = 4'd8;
	localparam [1:0] __const__num_inports_at_update_recv_required_vector = 2'd2;
	localparam [3:0] __const__num_outports_at_update_recv_required_vector = 4'd8;
	localparam [3:0] __const__num_outports_at_update_send_required_vector = 4'd8;
	reg [0:0] all_send_accepted;
	reg [7:0] during_prologue_allowing_vector;
	reg [1:0] in_dir [0:7];
	reg [0:0] in_dir_local [0:7];
	wire [2:0] prologue_count_wire [0:47][0:1];
	reg [2:0] prologue_counter [0:47][0:1];
	reg [2:0] prologue_counter_next [0:47][0:1];
	wire [34:0] recv_data_msg [0:1];
	wire [0:0] recv_data_val [0:1];
	reg [1:0] recv_required_vector;
	reg [7:0] recv_valid_excluding_prologue_vector;
	reg [7:0] recv_valid_or_during_prologue_allowing_vector;
	reg [7:0] send_accepted;
	reg [7:0] send_accepted_next;
	reg [7:0] send_rdy_or_during_prologue_vector;
	reg [7:0] send_required_vector;
	function automatic [2:0] sv2v_cast_3;
		input reg [2:0] inp;
		sv2v_cast_3 = inp;
	endfunction
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : update_in_dir_vector
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_in_dir_vector; i = i + 1'd1)
				begin
					in_dir[sv2v_cast_3(i)] = 2'd0;
					in_dir_local[sv2v_cast_3(i)] = 1'd0;
				end
		end
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_in_dir_vector; i = i + 1'd1)
				begin
					in_dir[sv2v_cast_3(i)] = crossbar_outport[(7 - sv2v_cast_3(i)) * 2+:2];
					if (in_dir[sv2v_cast_3(i)] > 2'd0)
						in_dir_local[sv2v_cast_3(i)] = sv2v_cast_1(in_dir[sv2v_cast_3(i)] - 2'd1);
				end
		end
	end
	always @(*) begin : update_prologue_allowing_vector
		if (_sv2v_0)
			;
		during_prologue_allowing_vector = 8'd0;
		begin : sv2v_autoblock_3
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_prologue_allowing_vector; i = i + 1'd1)
				if (in_dir[sv2v_cast_3(i)] > 2'd0)
					during_prologue_allowing_vector[sv2v_cast_3(i)] = prologue_counter[ctrl_addr_inport][in_dir_local[sv2v_cast_3(i)]] < prologue_count_wire[ctrl_addr_inport][in_dir_local[sv2v_cast_3(i)]];
				else
					during_prologue_allowing_vector[sv2v_cast_3(i)] = 1'd0;
		end
	end
	function automatic [5:0] sv2v_cast_6;
		input reg [5:0] inp;
		sv2v_cast_6 = inp;
	endfunction
	always @(*) begin : update_prologue_counter_next
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_4
			reg [31:0] addr;
			for (addr = 1'd0; addr < __const__ctrl_mem_size_at_update_prologue_counter_next; addr = addr + 1'd1)
				begin : sv2v_autoblock_5
					reg [31:0] i;
					for (i = 1'd0; i < __const__num_inports_at_update_prologue_counter_next; i = i + 1'd1)
						begin
							prologue_counter_next[sv2v_cast_6(addr)][sv2v_cast_1(i)] = prologue_counter[sv2v_cast_6(addr)][sv2v_cast_1(i)];
							begin : sv2v_autoblock_6
								reg [31:0] j;
								for (j = 1'd0; j < __const__num_outports_at_update_prologue_counter_next; j = j + 1'd1)
									if ((((recv_opt__rdy & (in_dir[sv2v_cast_3(j)] > 2'd0)) & (in_dir_local[sv2v_cast_3(j)] == sv2v_cast_1(i))) & (sv2v_cast_6(addr) == ctrl_addr_inport)) & (prologue_counter[sv2v_cast_6(addr)][sv2v_cast_1(i)] < prologue_count_wire[sv2v_cast_6(addr)][sv2v_cast_1(i)]))
										prologue_counter_next[sv2v_cast_6(addr)][sv2v_cast_1(i)] = prologue_counter[sv2v_cast_6(addr)][sv2v_cast_1(i)] + 3'd1;
							end
						end
				end
		end
	end
	always @(*) begin : update_prologue_or_valid_vector
		if (_sv2v_0)
			;
		recv_valid_or_during_prologue_allowing_vector = 8'd0;
		begin : sv2v_autoblock_7
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_prologue_or_valid_vector; i = i + 1'd1)
				recv_valid_or_during_prologue_allowing_vector[sv2v_cast_3(i)] = recv_valid_excluding_prologue_vector[sv2v_cast_3(i)] | during_prologue_allowing_vector[sv2v_cast_3(i)];
		end
	end
	always @(*) begin : update_rdy_vector
		if (_sv2v_0)
			;
		send_rdy_or_during_prologue_vector = 8'd0;
		begin : sv2v_autoblock_8
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_rdy_vector; i = i + 1'd1)
				if ((in_dir[sv2v_cast_3(i)] > 2'd0) & (~compute_done | (sv2v_cast_3(i) < __const__outport_towards_local_base_id_at_update_rdy_vector)))
					send_rdy_or_during_prologue_vector[sv2v_cast_3(i)] = send_data__rdy[7 - sv2v_cast_3(i)+:1] | during_prologue_allowing_vector[sv2v_cast_3(i)];
				else
					send_rdy_or_during_prologue_vector[sv2v_cast_3(i)] = 1'd1;
		end
	end
	always @(*) begin : update_recv_required_vector
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_9
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_update_recv_required_vector; i = i + 1'd1)
				recv_required_vector[sv2v_cast_1(i)] = 1'd0;
		end
		begin : sv2v_autoblock_10
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_recv_required_vector; i = i + 1'd1)
				if ((in_dir[sv2v_cast_3(i)] > 2'd0) & ~during_prologue_allowing_vector[sv2v_cast_3(i)])
					recv_required_vector[in_dir_local[sv2v_cast_3(i)]] = 1'd1;
		end
	end
	always @(*) begin : update_send_accepted_next
		if (_sv2v_0)
			;
		send_accepted_next = send_accepted;
		if (recv_opt__val & (recv_opt__msg[110-:7] != __const__OPT_START)) begin
			begin : sv2v_autoblock_11
				reg [31:0] i;
				for (i = 1'd0; i < __const__num_outports_at_update_send_accepted_next; i = i + 1'd1)
					if (((send_required_vector[sv2v_cast_3(i)] & ~send_accepted[sv2v_cast_3(i)]) & send_data__rdy[7 - sv2v_cast_3(i)+:1]) & send_data__val[7 - sv2v_cast_3(i)+:1])
						send_accepted_next[sv2v_cast_3(i)] = 1'd1;
			end
			if (recv_opt__rdy)
				send_accepted_next = 8'd0;
		end
		else
			send_accepted_next = 8'd0;
	end
	always @(*) begin : update_send_required_vector
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_12
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_send_required_vector; i = i + 1'd1)
				send_required_vector[sv2v_cast_3(i)] = 1'd0;
		end
		begin : sv2v_autoblock_13
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_send_required_vector; i = i + 1'd1)
				if ((in_dir[sv2v_cast_3(i)] > 2'd0) & ~during_prologue_allowing_vector[sv2v_cast_3(i)])
					send_required_vector[sv2v_cast_3(i)] = 1'd1;
		end
	end
	always @(*) begin : update_signal
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_14
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_update_signal; i = i + 1'd1)
				recv_data__rdy[1 - sv2v_cast_1(i)+:1] = 1'd0;
		end
		begin : sv2v_autoblock_15
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_signal; i = i + 1'd1)
				begin
					send_data__val[7 - sv2v_cast_3(i)+:1] = 1'd0;
					send_data__msg[(7 - sv2v_cast_3(i)) * 35+:35] = 35'h000000000;
				end
		end
		recv_opt__rdy = 1'd0;
		all_send_accepted = 1'd0;
		if (recv_opt__val & (recv_opt__msg[110-:7] != __const__OPT_START)) begin
			all_send_accepted = 1'd1;
			begin : sv2v_autoblock_16
				reg [31:0] i;
				for (i = 1'd0; i < __const__num_outports_at_update_signal; i = i + 1'd1)
					if ((send_required_vector[sv2v_cast_3(i)] & ~send_accepted[sv2v_cast_3(i)]) & ~send_rdy_or_during_prologue_vector[sv2v_cast_3(i)])
						all_send_accepted = 1'd0;
			end
			begin : sv2v_autoblock_17
				reg [31:0] i;
				for (i = 1'd0; i < __const__num_inports_at_update_signal; i = i + 1'd1)
					recv_data__rdy[1 - sv2v_cast_1(i)+:1] = (&recv_valid_or_during_prologue_allowing_vector & all_send_accepted) & recv_required_vector[sv2v_cast_1(i)];
			end
			begin : sv2v_autoblock_18
				reg [31:0] i;
				for (i = 1'd0; i < __const__num_outports_at_update_signal; i = i + 1'd1)
					begin
						send_data__val[7 - sv2v_cast_3(i)+:1] = (&recv_valid_or_during_prologue_allowing_vector & send_required_vector[sv2v_cast_3(i)]) & ~send_accepted[sv2v_cast_3(i)];
						if ((&recv_valid_or_during_prologue_allowing_vector & send_required_vector[sv2v_cast_3(i)]) & ~send_accepted[sv2v_cast_3(i)]) begin
							send_data__msg[((7 - sv2v_cast_3(i)) * 35) + 34-:32] = recv_data_msg[in_dir_local[sv2v_cast_3(i)]][34-:32];
							send_data__msg[((7 - sv2v_cast_3(i)) * 35) + 2] = recv_data_msg[in_dir_local[sv2v_cast_3(i)]][2];
						end
					end
			end
			recv_opt__rdy = all_send_accepted & (&recv_valid_or_during_prologue_allowing_vector);
		end
	end
	always @(*) begin : update_valid_vector
		if (_sv2v_0)
			;
		recv_valid_excluding_prologue_vector = 8'd0;
		begin : sv2v_autoblock_19
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_valid_vector; i = i + 1'd1)
				if (in_dir[sv2v_cast_3(i)] > 2'd0)
					recv_valid_excluding_prologue_vector[sv2v_cast_3(i)] = recv_data_val[in_dir_local[sv2v_cast_3(i)]] & ~during_prologue_allowing_vector[sv2v_cast_3(i)];
				else
					recv_valid_excluding_prologue_vector[sv2v_cast_3(i)] = 1'd1;
		end
	end
	always @(posedge clk) begin : update_prologue_counter
		if (reset | clear) begin
			begin : sv2v_autoblock_20
				reg [31:0] addr;
				for (addr = 1'd0; addr < __const__ctrl_mem_size_at_update_prologue_counter; addr = addr + 1'd1)
					begin : sv2v_autoblock_21
						reg [31:0] i;
						for (i = 1'd0; i < __const__num_inports_at_update_prologue_counter; i = i + 1'd1)
							prologue_counter[sv2v_cast_6(addr)][sv2v_cast_1(i)] <= 3'd0;
					end
			end
			send_accepted <= 8'd0;
		end
		else begin
			begin : sv2v_autoblock_22
				reg [31:0] addr;
				for (addr = 1'd0; addr < __const__ctrl_mem_size_at_update_prologue_counter; addr = addr + 1'd1)
					begin : sv2v_autoblock_23
						reg [31:0] i;
						for (i = 1'd0; i < __const__num_inports_at_update_prologue_counter; i = i + 1'd1)
							prologue_counter[sv2v_cast_6(addr)][sv2v_cast_1(i)] <= prologue_counter_next[sv2v_cast_6(addr)][sv2v_cast_1(i)];
					end
			end
			send_accepted <= send_accepted_next;
		end
	end
	assign recv_data_msg[0] = recv_data__msg[35+:35];
	assign recv_data_val[0] = recv_data__val[1+:1];
	assign recv_data_msg[1] = recv_data__msg[0+:35];
	assign recv_data_val[1] = recv_data__val[0+:1];
	assign prologue_count_wire[0][0] = prologue_count_inport[285+:3];
	assign prologue_count_wire[0][1] = prologue_count_inport[282+:3];
	assign prologue_count_wire[1][0] = prologue_count_inport[279+:3];
	assign prologue_count_wire[1][1] = prologue_count_inport[276+:3];
	assign prologue_count_wire[2][0] = prologue_count_inport[273+:3];
	assign prologue_count_wire[2][1] = prologue_count_inport[270+:3];
	assign prologue_count_wire[3][0] = prologue_count_inport[267+:3];
	assign prologue_count_wire[3][1] = prologue_count_inport[264+:3];
	assign prologue_count_wire[4][0] = prologue_count_inport[261+:3];
	assign prologue_count_wire[4][1] = prologue_count_inport[258+:3];
	assign prologue_count_wire[5][0] = prologue_count_inport[255+:3];
	assign prologue_count_wire[5][1] = prologue_count_inport[252+:3];
	assign prologue_count_wire[6][0] = prologue_count_inport[249+:3];
	assign prologue_count_wire[6][1] = prologue_count_inport[246+:3];
	assign prologue_count_wire[7][0] = prologue_count_inport[243+:3];
	assign prologue_count_wire[7][1] = prologue_count_inport[240+:3];
	assign prologue_count_wire[8][0] = prologue_count_inport[237+:3];
	assign prologue_count_wire[8][1] = prologue_count_inport[234+:3];
	assign prologue_count_wire[9][0] = prologue_count_inport[231+:3];
	assign prologue_count_wire[9][1] = prologue_count_inport[228+:3];
	assign prologue_count_wire[10][0] = prologue_count_inport[225+:3];
	assign prologue_count_wire[10][1] = prologue_count_inport[222+:3];
	assign prologue_count_wire[11][0] = prologue_count_inport[219+:3];
	assign prologue_count_wire[11][1] = prologue_count_inport[216+:3];
	assign prologue_count_wire[12][0] = prologue_count_inport[213+:3];
	assign prologue_count_wire[12][1] = prologue_count_inport[210+:3];
	assign prologue_count_wire[13][0] = prologue_count_inport[207+:3];
	assign prologue_count_wire[13][1] = prologue_count_inport[204+:3];
	assign prologue_count_wire[14][0] = prologue_count_inport[201+:3];
	assign prologue_count_wire[14][1] = prologue_count_inport[198+:3];
	assign prologue_count_wire[15][0] = prologue_count_inport[195+:3];
	assign prologue_count_wire[15][1] = prologue_count_inport[192+:3];
	assign prologue_count_wire[16][0] = prologue_count_inport[189+:3];
	assign prologue_count_wire[16][1] = prologue_count_inport[186+:3];
	assign prologue_count_wire[17][0] = prologue_count_inport[183+:3];
	assign prologue_count_wire[17][1] = prologue_count_inport[180+:3];
	assign prologue_count_wire[18][0] = prologue_count_inport[177+:3];
	assign prologue_count_wire[18][1] = prologue_count_inport[174+:3];
	assign prologue_count_wire[19][0] = prologue_count_inport[171+:3];
	assign prologue_count_wire[19][1] = prologue_count_inport[168+:3];
	assign prologue_count_wire[20][0] = prologue_count_inport[165+:3];
	assign prologue_count_wire[20][1] = prologue_count_inport[162+:3];
	assign prologue_count_wire[21][0] = prologue_count_inport[159+:3];
	assign prologue_count_wire[21][1] = prologue_count_inport[156+:3];
	assign prologue_count_wire[22][0] = prologue_count_inport[153+:3];
	assign prologue_count_wire[22][1] = prologue_count_inport[150+:3];
	assign prologue_count_wire[23][0] = prologue_count_inport[147+:3];
	assign prologue_count_wire[23][1] = prologue_count_inport[144+:3];
	assign prologue_count_wire[24][0] = prologue_count_inport[141+:3];
	assign prologue_count_wire[24][1] = prologue_count_inport[138+:3];
	assign prologue_count_wire[25][0] = prologue_count_inport[135+:3];
	assign prologue_count_wire[25][1] = prologue_count_inport[132+:3];
	assign prologue_count_wire[26][0] = prologue_count_inport[129+:3];
	assign prologue_count_wire[26][1] = prologue_count_inport[126+:3];
	assign prologue_count_wire[27][0] = prologue_count_inport[123+:3];
	assign prologue_count_wire[27][1] = prologue_count_inport[120+:3];
	assign prologue_count_wire[28][0] = prologue_count_inport[117+:3];
	assign prologue_count_wire[28][1] = prologue_count_inport[114+:3];
	assign prologue_count_wire[29][0] = prologue_count_inport[111+:3];
	assign prologue_count_wire[29][1] = prologue_count_inport[108+:3];
	assign prologue_count_wire[30][0] = prologue_count_inport[105+:3];
	assign prologue_count_wire[30][1] = prologue_count_inport[102+:3];
	assign prologue_count_wire[31][0] = prologue_count_inport[99+:3];
	assign prologue_count_wire[31][1] = prologue_count_inport[96+:3];
	assign prologue_count_wire[32][0] = prologue_count_inport[93+:3];
	assign prologue_count_wire[32][1] = prologue_count_inport[90+:3];
	assign prologue_count_wire[33][0] = prologue_count_inport[87+:3];
	assign prologue_count_wire[33][1] = prologue_count_inport[84+:3];
	assign prologue_count_wire[34][0] = prologue_count_inport[81+:3];
	assign prologue_count_wire[34][1] = prologue_count_inport[78+:3];
	assign prologue_count_wire[35][0] = prologue_count_inport[75+:3];
	assign prologue_count_wire[35][1] = prologue_count_inport[72+:3];
	assign prologue_count_wire[36][0] = prologue_count_inport[69+:3];
	assign prologue_count_wire[36][1] = prologue_count_inport[66+:3];
	assign prologue_count_wire[37][0] = prologue_count_inport[63+:3];
	assign prologue_count_wire[37][1] = prologue_count_inport[60+:3];
	assign prologue_count_wire[38][0] = prologue_count_inport[57+:3];
	assign prologue_count_wire[38][1] = prologue_count_inport[54+:3];
	assign prologue_count_wire[39][0] = prologue_count_inport[51+:3];
	assign prologue_count_wire[39][1] = prologue_count_inport[48+:3];
	assign prologue_count_wire[40][0] = prologue_count_inport[45+:3];
	assign prologue_count_wire[40][1] = prologue_count_inport[42+:3];
	assign prologue_count_wire[41][0] = prologue_count_inport[39+:3];
	assign prologue_count_wire[41][1] = prologue_count_inport[36+:3];
	assign prologue_count_wire[42][0] = prologue_count_inport[33+:3];
	assign prologue_count_wire[42][1] = prologue_count_inport[30+:3];
	assign prologue_count_wire[43][0] = prologue_count_inport[27+:3];
	assign prologue_count_wire[43][1] = prologue_count_inport[24+:3];
	assign prologue_count_wire[44][0] = prologue_count_inport[21+:3];
	assign prologue_count_wire[44][1] = prologue_count_inport[18+:3];
	assign prologue_count_wire[45][0] = prologue_count_inport[15+:3];
	assign prologue_count_wire[45][1] = prologue_count_inport[12+:3];
	assign prologue_count_wire[46][0] = prologue_count_inport[9+:3];
	assign prologue_count_wire[46][1] = prologue_count_inport[6+:3];
	assign prologue_count_wire[47][0] = prologue_count_inport[3+:3];
	assign prologue_count_wire[47][1] = prologue_count_inport[0+:3];
	initial _sv2v_0 = 0;
endmodule
module RegisterFile__14fb828f022f9b03 (
	clk,
	raddr,
	rdata,
	reset,
	waddr,
	wdata,
	wen
);
	reg _sv2v_0;
	input wire [0:0] clk;
	input wire [2:0] raddr;
	output reg [34:0] rdata;
	input wire [0:0] reset;
	input wire [2:0] waddr;
	input wire [34:0] wdata;
	input wire [0:0] wen;
	localparam [0:0] __const__rd_ports_at_up_rf_read = 1'd1;
	localparam [0:0] __const__wr_ports_at_up_rf_write = 1'd1;
	reg [34:0] regs [0:7];
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : up_rf_read
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__rd_ports_at_up_rf_read; i = i + 1'd1)
				rdata[sv2v_cast_1(i) * 35+:35] = regs[raddr[sv2v_cast_1(i) * 3+:3]];
		end
	end
	always @(posedge clk) begin : up_rf_write
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__wr_ports_at_up_rf_write; i = i + 1'd1)
				if (wen[sv2v_cast_1(i)+:1])
					regs[waddr[sv2v_cast_1(i) * 3+:3]] <= wdata[sv2v_cast_1(i) * 35+:35];
		end
	end
	initial _sv2v_0 = 0;
endmodule
module RegisterBankRTL__493e7203765a09b1 (
	clear,
	clk,
	inport_ctrl_proceed,
	inport_opt,
	inport_valid,
	inport_wdata,
	outport_wr_rdy,
	reset,
	send_data_to_fu__msg,
	send_data_to_fu__rdy,
	send_data_to_fu__val,
	send_data_to_xbar__msg,
	send_data_to_xbar__rdy,
	send_data_to_xbar__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [0:0] inport_ctrl_proceed;
	input wire [110:0] inport_opt;
	input wire [2:0] inport_valid;
	input wire [104:0] inport_wdata;
	output reg [0:0] outport_wr_rdy;
	input wire [0:0] reset;
	output reg [34:0] send_data_to_fu__msg;
	input wire [0:0] send_data_to_fu__rdy;
	output reg [0:0] send_data_to_fu__val;
	output reg [34:0] send_data_to_xbar__msg;
	input wire [0:0] send_data_to_xbar__rdy;
	output reg [0:0] send_data_to_xbar__val;
	localparam [0:0] __const__reg_bank_id_at_update_token_status = 1'd0;
	localparam [0:0] __const__READ_TOWARDS_FU = 1'd1;
	localparam [1:0] __const__READ_TOWARDS_BOTH = 2'd3;
	localparam [1:0] __const__READ_TOWARDS_ROUTING_XBAR = 2'd2;
	localparam [3:0] __const__num_registers_at_update_token_status = 4'd8;
	localparam [0:0] __const__reg_bank_id_at_access_registers = 1'd0;
	localparam [3:0] __const__num_registers_at_update_token_masks = 4'd8;
	localparam [0:0] __const__reg_bank_id_at_update_token_masks = 1'd0;
	localparam [0:0] __const__reg_bank_id_at_update_token_valid = 1'd0;
	reg [7:0] armed;
	reg [0:0] read_armed;
	reg [0:0] read_token_valid;
	reg [0:0] read_towards_fu;
	reg [0:0] read_towards_xbar;
	reg [0:0] skid_commit;
	reg [34:0] skid_data;
	reg [2:0] skid_idx;
	reg [0:0] skid_park;
	reg [0:0] skid_target_being_read;
	reg [0:0] skid_target_holds_token;
	reg [0:0] skid_valid;
	reg [7:0] token_clear_mask;
	reg [7:0] token_set_mask;
	reg [7:0] token_valid;
	reg [0:0] wr_accept;
	reg [0:0] wr_en;
	reg [34:0] wr_sel_data;
	reg [0:0] wr_target_being_read;
	reg [0:0] wr_target_holds_token;
	wire [0:0] reg_file__clk;
	reg [2:0] reg_file__raddr;
	wire [34:0] reg_file__rdata;
	wire [0:0] reg_file__reset;
	reg [2:0] reg_file__waddr;
	reg [34:0] reg_file__wdata;
	reg [0:0] reg_file__wen;
	RegisterFile__14fb828f022f9b03 reg_file(
		.clk(reg_file__clk),
		.raddr(reg_file__raddr),
		.rdata(reg_file__rdata),
		.reset(reg_file__reset),
		.waddr(reg_file__waddr),
		.wdata(reg_file__wdata),
		.wen(reg_file__wen)
	);
	reg [1:0] __tmpvar__update_token_status_read_towards;
	reg [1:0] __tmpvar__access_registers_read_towards;
	reg [1:0] __tmpvar__access_registers_write_reg_from;
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	always @(*) begin : access_registers
		if (_sv2v_0)
			;
		reg_file__raddr[1'd0 * 3+:3] = 3'd0;
		send_data_to_fu__msg = 35'h000000000;
		send_data_to_xbar__msg = 35'h000000000;
		reg_file__waddr[1'd0 * 3+:3] = 3'd0;
		reg_file__wdata[1'd0 * 35+:35] = 35'h000000000;
		reg_file__wen[1'd0+:1] = 1'd0;
		wr_accept = 1'd0;
		wr_en = 1'd0;
		skid_park = 1'd0;
		wr_sel_data = 35'h000000000;
		__tmpvar__access_registers_read_towards = inport_opt[12 + (sv2v_cast_2(__const__reg_bank_id_at_access_registers) * 2)+:2];
		if (__tmpvar__access_registers_read_towards > 2'd0) begin
			reg_file__raddr[1'd0 * 3+:3] = inport_opt[0 + (sv2v_cast_2(__const__reg_bank_id_at_access_registers) * 3)+:3];
			send_data_to_fu__msg = reg_file__rdata[1'd0 * 35+:35];
			send_data_to_xbar__msg = reg_file__rdata[1'd0 * 35+:35];
		end
		__tmpvar__access_registers_write_reg_from = inport_opt[32 + (sv2v_cast_2(__const__reg_bank_id_at_access_registers) * 2)+:2];
		if (~reset & (__tmpvar__access_registers_write_reg_from > 2'd0)) begin
			wr_sel_data = inport_wdata[((2 + 2'd1) - __tmpvar__access_registers_write_reg_from) * 35+:35];
			if (inport_valid[(2 + 2'd1) - __tmpvar__access_registers_write_reg_from+:1] & outport_wr_rdy) begin
				wr_accept = 1'd1;
				if ((~wr_target_holds_token & ~wr_target_being_read) | (wr_target_being_read & inport_ctrl_proceed))
					wr_en = 1'd1;
				else
					skid_park = 1'd1;
			end
		end
		if (skid_commit) begin
			reg_file__waddr[1'd0 * 3+:3] = skid_idx;
			reg_file__wdata[1'd0 * 35+:35] = skid_data;
			reg_file__wen[1'd0+:1] = 1'd1;
		end
		else if (wr_en) begin
			reg_file__waddr[1'd0 * 3+:3] = inport_opt[20 + (sv2v_cast_2(__const__reg_bank_id_at_access_registers) * 3)+:3];
			reg_file__wdata[1'd0 * 35+:35] = wr_sel_data;
			reg_file__wen[1'd0+:1] = 1'd1;
		end
	end
	always @(*) begin : update_send_val
		if (_sv2v_0)
			;
		send_data_to_fu__val = (~reset & read_towards_fu) & (read_token_valid | ~read_armed);
		send_data_to_xbar__val = (~reset & read_towards_xbar) & (read_token_valid | ~read_armed);
	end
	function automatic [2:0] sv2v_cast_3;
		input reg [2:0] inp;
		sv2v_cast_3 = inp;
	endfunction
	always @(*) begin : update_token_masks
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] r;
			for (r = 1'd0; r < __const__num_registers_at_update_token_masks; r = r + 1'd1)
				begin
					token_set_mask[sv2v_cast_3(r)] = (wr_en & (inport_opt[20 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_masks) * 3)+:3] == sv2v_cast_3(r))) | (skid_commit & (skid_idx == sv2v_cast_3(r)));
					token_clear_mask[sv2v_cast_3(r)] = ((inport_ctrl_proceed & (read_towards_fu | read_towards_xbar)) & (inport_opt[0 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_masks) * 3)+:3] == sv2v_cast_3(r))) & token_valid[sv2v_cast_3(r)];
				end
		end
	end
	always @(*) begin : update_token_status
		if (_sv2v_0)
			;
		__tmpvar__update_token_status_read_towards = inport_opt[12 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_status) * 2)+:2];
		read_towards_fu = (__tmpvar__update_token_status_read_towards == sv2v_cast_2(__const__READ_TOWARDS_FU)) | (__tmpvar__update_token_status_read_towards == __const__READ_TOWARDS_BOTH);
		read_towards_xbar = (__tmpvar__update_token_status_read_towards == __const__READ_TOWARDS_ROUTING_XBAR) | (__tmpvar__update_token_status_read_towards == __const__READ_TOWARDS_BOTH);
		read_token_valid = 1'd0;
		read_armed = 1'd0;
		wr_target_holds_token = 1'd0;
		skid_target_holds_token = 1'd0;
		begin : sv2v_autoblock_2
			reg [31:0] r;
			for (r = 1'd0; r < __const__num_registers_at_update_token_status; r = r + 1'd1)
				begin
					if (inport_opt[0 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_status) * 3)+:3] == sv2v_cast_3(r)) begin
						read_token_valid = token_valid[sv2v_cast_3(r)];
						read_armed = armed[sv2v_cast_3(r)];
					end
					if (inport_opt[20 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_status) * 3)+:3] == sv2v_cast_3(r))
						wr_target_holds_token = token_valid[sv2v_cast_3(r)];
					if (skid_idx == sv2v_cast_3(r))
						skid_target_holds_token = token_valid[sv2v_cast_3(r)];
				end
		end
		wr_target_being_read = (read_towards_fu | read_towards_xbar) & (inport_opt[0 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_status) * 3)+:3] == inport_opt[20 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_status) * 3)+:3]);
		skid_target_being_read = (read_towards_fu | read_towards_xbar) & (inport_opt[0 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_status) * 3)+:3] == skid_idx);
		skid_commit = skid_valid & ((skid_target_being_read & inport_ctrl_proceed) | (~skid_target_being_read & ~skid_target_holds_token));
		outport_wr_rdy = (~wr_target_holds_token & ~wr_target_being_read) | (wr_target_being_read & ~skid_valid);
	end
	always @(posedge clk) begin : update_token_valid
		if (reset | clear) begin
			token_valid <= 8'd0;
			armed <= 8'd0;
			skid_valid <= 1'd0;
		end
		else begin
			token_valid <= (token_valid & ~token_clear_mask) | token_set_mask;
			armed <= armed | token_set_mask;
			if (skid_park) begin
				skid_valid <= 1'd1;
				skid_data <= wr_sel_data;
				skid_idx <= inport_opt[20 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_valid) * 3)+:3];
			end
			else if (skid_commit)
				skid_valid <= 1'd0;
		end
	end
	assign reg_file__clk = clk;
	assign reg_file__reset = reset;
	initial _sv2v_0 = 0;
endmodule
module RegisterBankRTL__3596ca3621cddc37 (
	clear,
	clk,
	inport_ctrl_proceed,
	inport_opt,
	inport_valid,
	inport_wdata,
	outport_wr_rdy,
	reset,
	send_data_to_fu__msg,
	send_data_to_fu__rdy,
	send_data_to_fu__val,
	send_data_to_xbar__msg,
	send_data_to_xbar__rdy,
	send_data_to_xbar__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [0:0] inport_ctrl_proceed;
	input wire [110:0] inport_opt;
	input wire [2:0] inport_valid;
	input wire [104:0] inport_wdata;
	output reg [0:0] outport_wr_rdy;
	input wire [0:0] reset;
	output reg [34:0] send_data_to_fu__msg;
	input wire [0:0] send_data_to_fu__rdy;
	output reg [0:0] send_data_to_fu__val;
	output reg [34:0] send_data_to_xbar__msg;
	input wire [0:0] send_data_to_xbar__rdy;
	output reg [0:0] send_data_to_xbar__val;
	localparam [0:0] __const__reg_bank_id_at_update_token_status = 1'd1;
	localparam [0:0] __const__READ_TOWARDS_FU = 1'd1;
	localparam [1:0] __const__READ_TOWARDS_BOTH = 2'd3;
	localparam [1:0] __const__READ_TOWARDS_ROUTING_XBAR = 2'd2;
	localparam [3:0] __const__num_registers_at_update_token_status = 4'd8;
	localparam [0:0] __const__reg_bank_id_at_access_registers = 1'd1;
	localparam [3:0] __const__num_registers_at_update_token_masks = 4'd8;
	localparam [0:0] __const__reg_bank_id_at_update_token_masks = 1'd1;
	localparam [0:0] __const__reg_bank_id_at_update_token_valid = 1'd1;
	reg [7:0] armed;
	reg [0:0] read_armed;
	reg [0:0] read_token_valid;
	reg [0:0] read_towards_fu;
	reg [0:0] read_towards_xbar;
	reg [0:0] skid_commit;
	reg [34:0] skid_data;
	reg [2:0] skid_idx;
	reg [0:0] skid_park;
	reg [0:0] skid_target_being_read;
	reg [0:0] skid_target_holds_token;
	reg [0:0] skid_valid;
	reg [7:0] token_clear_mask;
	reg [7:0] token_set_mask;
	reg [7:0] token_valid;
	reg [0:0] wr_accept;
	reg [0:0] wr_en;
	reg [34:0] wr_sel_data;
	reg [0:0] wr_target_being_read;
	reg [0:0] wr_target_holds_token;
	wire [0:0] reg_file__clk;
	reg [2:0] reg_file__raddr;
	wire [34:0] reg_file__rdata;
	wire [0:0] reg_file__reset;
	reg [2:0] reg_file__waddr;
	reg [34:0] reg_file__wdata;
	reg [0:0] reg_file__wen;
	RegisterFile__14fb828f022f9b03 reg_file(
		.clk(reg_file__clk),
		.raddr(reg_file__raddr),
		.rdata(reg_file__rdata),
		.reset(reg_file__reset),
		.waddr(reg_file__waddr),
		.wdata(reg_file__wdata),
		.wen(reg_file__wen)
	);
	reg [1:0] __tmpvar__update_token_status_read_towards;
	reg [1:0] __tmpvar__access_registers_read_towards;
	reg [1:0] __tmpvar__access_registers_write_reg_from;
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	always @(*) begin : access_registers
		if (_sv2v_0)
			;
		reg_file__raddr[1'd0 * 3+:3] = 3'd0;
		send_data_to_fu__msg = 35'h000000000;
		send_data_to_xbar__msg = 35'h000000000;
		reg_file__waddr[1'd0 * 3+:3] = 3'd0;
		reg_file__wdata[1'd0 * 35+:35] = 35'h000000000;
		reg_file__wen[1'd0+:1] = 1'd0;
		wr_accept = 1'd0;
		wr_en = 1'd0;
		skid_park = 1'd0;
		wr_sel_data = 35'h000000000;
		__tmpvar__access_registers_read_towards = inport_opt[12 + (sv2v_cast_2(__const__reg_bank_id_at_access_registers) * 2)+:2];
		if (__tmpvar__access_registers_read_towards > 2'd0) begin
			reg_file__raddr[1'd0 * 3+:3] = inport_opt[0 + (sv2v_cast_2(__const__reg_bank_id_at_access_registers) * 3)+:3];
			send_data_to_fu__msg = reg_file__rdata[1'd0 * 35+:35];
			send_data_to_xbar__msg = reg_file__rdata[1'd0 * 35+:35];
		end
		__tmpvar__access_registers_write_reg_from = inport_opt[32 + (sv2v_cast_2(__const__reg_bank_id_at_access_registers) * 2)+:2];
		if (~reset & (__tmpvar__access_registers_write_reg_from > 2'd0)) begin
			wr_sel_data = inport_wdata[((2 + 2'd1) - __tmpvar__access_registers_write_reg_from) * 35+:35];
			if (inport_valid[(2 + 2'd1) - __tmpvar__access_registers_write_reg_from+:1] & outport_wr_rdy) begin
				wr_accept = 1'd1;
				if ((~wr_target_holds_token & ~wr_target_being_read) | (wr_target_being_read & inport_ctrl_proceed))
					wr_en = 1'd1;
				else
					skid_park = 1'd1;
			end
		end
		if (skid_commit) begin
			reg_file__waddr[1'd0 * 3+:3] = skid_idx;
			reg_file__wdata[1'd0 * 35+:35] = skid_data;
			reg_file__wen[1'd0+:1] = 1'd1;
		end
		else if (wr_en) begin
			reg_file__waddr[1'd0 * 3+:3] = inport_opt[20 + (sv2v_cast_2(__const__reg_bank_id_at_access_registers) * 3)+:3];
			reg_file__wdata[1'd0 * 35+:35] = wr_sel_data;
			reg_file__wen[1'd0+:1] = 1'd1;
		end
	end
	always @(*) begin : update_send_val
		if (_sv2v_0)
			;
		send_data_to_fu__val = (~reset & read_towards_fu) & (read_token_valid | ~read_armed);
		send_data_to_xbar__val = (~reset & read_towards_xbar) & (read_token_valid | ~read_armed);
	end
	function automatic [2:0] sv2v_cast_3;
		input reg [2:0] inp;
		sv2v_cast_3 = inp;
	endfunction
	always @(*) begin : update_token_masks
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] r;
			for (r = 1'd0; r < __const__num_registers_at_update_token_masks; r = r + 1'd1)
				begin
					token_set_mask[sv2v_cast_3(r)] = (wr_en & (inport_opt[20 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_masks) * 3)+:3] == sv2v_cast_3(r))) | (skid_commit & (skid_idx == sv2v_cast_3(r)));
					token_clear_mask[sv2v_cast_3(r)] = ((inport_ctrl_proceed & (read_towards_fu | read_towards_xbar)) & (inport_opt[0 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_masks) * 3)+:3] == sv2v_cast_3(r))) & token_valid[sv2v_cast_3(r)];
				end
		end
	end
	always @(*) begin : update_token_status
		if (_sv2v_0)
			;
		__tmpvar__update_token_status_read_towards = inport_opt[12 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_status) * 2)+:2];
		read_towards_fu = (__tmpvar__update_token_status_read_towards == sv2v_cast_2(__const__READ_TOWARDS_FU)) | (__tmpvar__update_token_status_read_towards == __const__READ_TOWARDS_BOTH);
		read_towards_xbar = (__tmpvar__update_token_status_read_towards == __const__READ_TOWARDS_ROUTING_XBAR) | (__tmpvar__update_token_status_read_towards == __const__READ_TOWARDS_BOTH);
		read_token_valid = 1'd0;
		read_armed = 1'd0;
		wr_target_holds_token = 1'd0;
		skid_target_holds_token = 1'd0;
		begin : sv2v_autoblock_2
			reg [31:0] r;
			for (r = 1'd0; r < __const__num_registers_at_update_token_status; r = r + 1'd1)
				begin
					if (inport_opt[0 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_status) * 3)+:3] == sv2v_cast_3(r)) begin
						read_token_valid = token_valid[sv2v_cast_3(r)];
						read_armed = armed[sv2v_cast_3(r)];
					end
					if (inport_opt[20 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_status) * 3)+:3] == sv2v_cast_3(r))
						wr_target_holds_token = token_valid[sv2v_cast_3(r)];
					if (skid_idx == sv2v_cast_3(r))
						skid_target_holds_token = token_valid[sv2v_cast_3(r)];
				end
		end
		wr_target_being_read = (read_towards_fu | read_towards_xbar) & (inport_opt[0 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_status) * 3)+:3] == inport_opt[20 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_status) * 3)+:3]);
		skid_target_being_read = (read_towards_fu | read_towards_xbar) & (inport_opt[0 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_status) * 3)+:3] == skid_idx);
		skid_commit = skid_valid & ((skid_target_being_read & inport_ctrl_proceed) | (~skid_target_being_read & ~skid_target_holds_token));
		outport_wr_rdy = (~wr_target_holds_token & ~wr_target_being_read) | (wr_target_being_read & ~skid_valid);
	end
	always @(posedge clk) begin : update_token_valid
		if (reset | clear) begin
			token_valid <= 8'd0;
			armed <= 8'd0;
			skid_valid <= 1'd0;
		end
		else begin
			token_valid <= (token_valid & ~token_clear_mask) | token_set_mask;
			armed <= armed | token_set_mask;
			if (skid_park) begin
				skid_valid <= 1'd1;
				skid_data <= wr_sel_data;
				skid_idx <= inport_opt[20 + (sv2v_cast_2(__const__reg_bank_id_at_update_token_valid) * 3)+:3];
			end
			else if (skid_commit)
				skid_valid <= 1'd0;
		end
	end
	assign reg_file__clk = clk;
	assign reg_file__reset = reset;
	initial _sv2v_0 = 0;
endmodule
module RegisterBankRTL__c9691accc64d09b9 (
	clear,
	clk,
	inport_ctrl_proceed,
	inport_opt,
	inport_valid,
	inport_wdata,
	outport_wr_rdy,
	reset,
	send_data_to_fu__msg,
	send_data_to_fu__rdy,
	send_data_to_fu__val,
	send_data_to_xbar__msg,
	send_data_to_xbar__rdy,
	send_data_to_xbar__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [0:0] inport_ctrl_proceed;
	input wire [110:0] inport_opt;
	input wire [2:0] inport_valid;
	input wire [104:0] inport_wdata;
	output reg [0:0] outport_wr_rdy;
	input wire [0:0] reset;
	output reg [34:0] send_data_to_fu__msg;
	input wire [0:0] send_data_to_fu__rdy;
	output reg [0:0] send_data_to_fu__val;
	output reg [34:0] send_data_to_xbar__msg;
	input wire [0:0] send_data_to_xbar__rdy;
	output reg [0:0] send_data_to_xbar__val;
	localparam [1:0] __const__reg_bank_id_at_update_token_status = 2'd2;
	localparam [0:0] __const__READ_TOWARDS_FU = 1'd1;
	localparam [1:0] __const__READ_TOWARDS_BOTH = 2'd3;
	localparam [1:0] __const__READ_TOWARDS_ROUTING_XBAR = 2'd2;
	localparam [3:0] __const__num_registers_at_update_token_status = 4'd8;
	localparam [1:0] __const__reg_bank_id_at_access_registers = 2'd2;
	localparam [3:0] __const__num_registers_at_update_token_masks = 4'd8;
	localparam [1:0] __const__reg_bank_id_at_update_token_masks = 2'd2;
	localparam [1:0] __const__reg_bank_id_at_update_token_valid = 2'd2;
	reg [7:0] armed;
	reg [0:0] read_armed;
	reg [0:0] read_token_valid;
	reg [0:0] read_towards_fu;
	reg [0:0] read_towards_xbar;
	reg [0:0] skid_commit;
	reg [34:0] skid_data;
	reg [2:0] skid_idx;
	reg [0:0] skid_park;
	reg [0:0] skid_target_being_read;
	reg [0:0] skid_target_holds_token;
	reg [0:0] skid_valid;
	reg [7:0] token_clear_mask;
	reg [7:0] token_set_mask;
	reg [7:0] token_valid;
	reg [0:0] wr_accept;
	reg [0:0] wr_en;
	reg [34:0] wr_sel_data;
	reg [0:0] wr_target_being_read;
	reg [0:0] wr_target_holds_token;
	wire [0:0] reg_file__clk;
	reg [2:0] reg_file__raddr;
	wire [34:0] reg_file__rdata;
	wire [0:0] reg_file__reset;
	reg [2:0] reg_file__waddr;
	reg [34:0] reg_file__wdata;
	reg [0:0] reg_file__wen;
	RegisterFile__14fb828f022f9b03 reg_file(
		.clk(reg_file__clk),
		.raddr(reg_file__raddr),
		.rdata(reg_file__rdata),
		.reset(reg_file__reset),
		.waddr(reg_file__waddr),
		.wdata(reg_file__wdata),
		.wen(reg_file__wen)
	);
	reg [1:0] __tmpvar__update_token_status_read_towards;
	reg [1:0] __tmpvar__access_registers_read_towards;
	reg [1:0] __tmpvar__access_registers_write_reg_from;
	always @(*) begin : access_registers
		if (_sv2v_0)
			;
		reg_file__raddr[1'd0 * 3+:3] = 3'd0;
		send_data_to_fu__msg = 35'h000000000;
		send_data_to_xbar__msg = 35'h000000000;
		reg_file__waddr[1'd0 * 3+:3] = 3'd0;
		reg_file__wdata[1'd0 * 35+:35] = 35'h000000000;
		reg_file__wen[1'd0+:1] = 1'd0;
		wr_accept = 1'd0;
		wr_en = 1'd0;
		skid_park = 1'd0;
		wr_sel_data = 35'h000000000;
		__tmpvar__access_registers_read_towards = inport_opt[12 + (__const__reg_bank_id_at_access_registers * 2)+:2];
		if (__tmpvar__access_registers_read_towards > 2'd0) begin
			reg_file__raddr[1'd0 * 3+:3] = inport_opt[0 + (__const__reg_bank_id_at_access_registers * 3)+:3];
			send_data_to_fu__msg = reg_file__rdata[1'd0 * 35+:35];
			send_data_to_xbar__msg = reg_file__rdata[1'd0 * 35+:35];
		end
		__tmpvar__access_registers_write_reg_from = inport_opt[32 + (__const__reg_bank_id_at_access_registers * 2)+:2];
		if (~reset & (__tmpvar__access_registers_write_reg_from > 2'd0)) begin
			wr_sel_data = inport_wdata[((2 + 2'd1) - __tmpvar__access_registers_write_reg_from) * 35+:35];
			if (inport_valid[(2 + 2'd1) - __tmpvar__access_registers_write_reg_from+:1] & outport_wr_rdy) begin
				wr_accept = 1'd1;
				if ((~wr_target_holds_token & ~wr_target_being_read) | (wr_target_being_read & inport_ctrl_proceed))
					wr_en = 1'd1;
				else
					skid_park = 1'd1;
			end
		end
		if (skid_commit) begin
			reg_file__waddr[1'd0 * 3+:3] = skid_idx;
			reg_file__wdata[1'd0 * 35+:35] = skid_data;
			reg_file__wen[1'd0+:1] = 1'd1;
		end
		else if (wr_en) begin
			reg_file__waddr[1'd0 * 3+:3] = inport_opt[20 + (__const__reg_bank_id_at_access_registers * 3)+:3];
			reg_file__wdata[1'd0 * 35+:35] = wr_sel_data;
			reg_file__wen[1'd0+:1] = 1'd1;
		end
	end
	always @(*) begin : update_send_val
		if (_sv2v_0)
			;
		send_data_to_fu__val = (~reset & read_towards_fu) & (read_token_valid | ~read_armed);
		send_data_to_xbar__val = (~reset & read_towards_xbar) & (read_token_valid | ~read_armed);
	end
	function automatic [2:0] sv2v_cast_3;
		input reg [2:0] inp;
		sv2v_cast_3 = inp;
	endfunction
	always @(*) begin : update_token_masks
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] r;
			for (r = 1'd0; r < __const__num_registers_at_update_token_masks; r = r + 1'd1)
				begin
					token_set_mask[sv2v_cast_3(r)] = (wr_en & (inport_opt[20 + (__const__reg_bank_id_at_update_token_masks * 3)+:3] == sv2v_cast_3(r))) | (skid_commit & (skid_idx == sv2v_cast_3(r)));
					token_clear_mask[sv2v_cast_3(r)] = ((inport_ctrl_proceed & (read_towards_fu | read_towards_xbar)) & (inport_opt[0 + (__const__reg_bank_id_at_update_token_masks * 3)+:3] == sv2v_cast_3(r))) & token_valid[sv2v_cast_3(r)];
				end
		end
	end
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	always @(*) begin : update_token_status
		if (_sv2v_0)
			;
		__tmpvar__update_token_status_read_towards = inport_opt[12 + (__const__reg_bank_id_at_update_token_status * 2)+:2];
		read_towards_fu = (__tmpvar__update_token_status_read_towards == sv2v_cast_2(__const__READ_TOWARDS_FU)) | (__tmpvar__update_token_status_read_towards == __const__READ_TOWARDS_BOTH);
		read_towards_xbar = (__tmpvar__update_token_status_read_towards == __const__READ_TOWARDS_ROUTING_XBAR) | (__tmpvar__update_token_status_read_towards == __const__READ_TOWARDS_BOTH);
		read_token_valid = 1'd0;
		read_armed = 1'd0;
		wr_target_holds_token = 1'd0;
		skid_target_holds_token = 1'd0;
		begin : sv2v_autoblock_2
			reg [31:0] r;
			for (r = 1'd0; r < __const__num_registers_at_update_token_status; r = r + 1'd1)
				begin
					if (inport_opt[0 + (__const__reg_bank_id_at_update_token_status * 3)+:3] == sv2v_cast_3(r)) begin
						read_token_valid = token_valid[sv2v_cast_3(r)];
						read_armed = armed[sv2v_cast_3(r)];
					end
					if (inport_opt[20 + (__const__reg_bank_id_at_update_token_status * 3)+:3] == sv2v_cast_3(r))
						wr_target_holds_token = token_valid[sv2v_cast_3(r)];
					if (skid_idx == sv2v_cast_3(r))
						skid_target_holds_token = token_valid[sv2v_cast_3(r)];
				end
		end
		wr_target_being_read = (read_towards_fu | read_towards_xbar) & (inport_opt[0 + (__const__reg_bank_id_at_update_token_status * 3)+:3] == inport_opt[20 + (__const__reg_bank_id_at_update_token_status * 3)+:3]);
		skid_target_being_read = (read_towards_fu | read_towards_xbar) & (inport_opt[0 + (__const__reg_bank_id_at_update_token_status * 3)+:3] == skid_idx);
		skid_commit = skid_valid & ((skid_target_being_read & inport_ctrl_proceed) | (~skid_target_being_read & ~skid_target_holds_token));
		outport_wr_rdy = (~wr_target_holds_token & ~wr_target_being_read) | (wr_target_being_read & ~skid_valid);
	end
	always @(posedge clk) begin : update_token_valid
		if (reset | clear) begin
			token_valid <= 8'd0;
			armed <= 8'd0;
			skid_valid <= 1'd0;
		end
		else begin
			token_valid <= (token_valid & ~token_clear_mask) | token_set_mask;
			armed <= armed | token_set_mask;
			if (skid_park) begin
				skid_valid <= 1'd1;
				skid_data <= wr_sel_data;
				skid_idx <= inport_opt[20 + (__const__reg_bank_id_at_update_token_valid * 3)+:3];
			end
			else if (skid_commit)
				skid_valid <= 1'd0;
		end
	end
	assign reg_file__clk = clk;
	assign reg_file__reset = reset;
	initial _sv2v_0 = 0;
endmodule
module RegisterBankRTL__9a44c4bf7f9388e0 (
	clear,
	clk,
	inport_ctrl_proceed,
	inport_opt,
	inport_valid,
	inport_wdata,
	outport_wr_rdy,
	reset,
	send_data_to_fu__msg,
	send_data_to_fu__rdy,
	send_data_to_fu__val,
	send_data_to_xbar__msg,
	send_data_to_xbar__rdy,
	send_data_to_xbar__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [0:0] inport_ctrl_proceed;
	input wire [110:0] inport_opt;
	input wire [2:0] inport_valid;
	input wire [104:0] inport_wdata;
	output reg [0:0] outport_wr_rdy;
	input wire [0:0] reset;
	output reg [34:0] send_data_to_fu__msg;
	input wire [0:0] send_data_to_fu__rdy;
	output reg [0:0] send_data_to_fu__val;
	output reg [34:0] send_data_to_xbar__msg;
	input wire [0:0] send_data_to_xbar__rdy;
	output reg [0:0] send_data_to_xbar__val;
	localparam [1:0] __const__reg_bank_id_at_update_token_status = 2'd3;
	localparam [0:0] __const__READ_TOWARDS_FU = 1'd1;
	localparam [1:0] __const__READ_TOWARDS_BOTH = 2'd3;
	localparam [1:0] __const__READ_TOWARDS_ROUTING_XBAR = 2'd2;
	localparam [3:0] __const__num_registers_at_update_token_status = 4'd8;
	localparam [1:0] __const__reg_bank_id_at_access_registers = 2'd3;
	localparam [3:0] __const__num_registers_at_update_token_masks = 4'd8;
	localparam [1:0] __const__reg_bank_id_at_update_token_masks = 2'd3;
	localparam [1:0] __const__reg_bank_id_at_update_token_valid = 2'd3;
	reg [7:0] armed;
	reg [0:0] read_armed;
	reg [0:0] read_token_valid;
	reg [0:0] read_towards_fu;
	reg [0:0] read_towards_xbar;
	reg [0:0] skid_commit;
	reg [34:0] skid_data;
	reg [2:0] skid_idx;
	reg [0:0] skid_park;
	reg [0:0] skid_target_being_read;
	reg [0:0] skid_target_holds_token;
	reg [0:0] skid_valid;
	reg [7:0] token_clear_mask;
	reg [7:0] token_set_mask;
	reg [7:0] token_valid;
	reg [0:0] wr_accept;
	reg [0:0] wr_en;
	reg [34:0] wr_sel_data;
	reg [0:0] wr_target_being_read;
	reg [0:0] wr_target_holds_token;
	wire [0:0] reg_file__clk;
	reg [2:0] reg_file__raddr;
	wire [34:0] reg_file__rdata;
	wire [0:0] reg_file__reset;
	reg [2:0] reg_file__waddr;
	reg [34:0] reg_file__wdata;
	reg [0:0] reg_file__wen;
	RegisterFile__14fb828f022f9b03 reg_file(
		.clk(reg_file__clk),
		.raddr(reg_file__raddr),
		.rdata(reg_file__rdata),
		.reset(reg_file__reset),
		.waddr(reg_file__waddr),
		.wdata(reg_file__wdata),
		.wen(reg_file__wen)
	);
	reg [1:0] __tmpvar__update_token_status_read_towards;
	reg [1:0] __tmpvar__access_registers_read_towards;
	reg [1:0] __tmpvar__access_registers_write_reg_from;
	always @(*) begin : access_registers
		if (_sv2v_0)
			;
		reg_file__raddr[1'd0 * 3+:3] = 3'd0;
		send_data_to_fu__msg = 35'h000000000;
		send_data_to_xbar__msg = 35'h000000000;
		reg_file__waddr[1'd0 * 3+:3] = 3'd0;
		reg_file__wdata[1'd0 * 35+:35] = 35'h000000000;
		reg_file__wen[1'd0+:1] = 1'd0;
		wr_accept = 1'd0;
		wr_en = 1'd0;
		skid_park = 1'd0;
		wr_sel_data = 35'h000000000;
		__tmpvar__access_registers_read_towards = inport_opt[12 + (__const__reg_bank_id_at_access_registers * 2)+:2];
		if (__tmpvar__access_registers_read_towards > 2'd0) begin
			reg_file__raddr[1'd0 * 3+:3] = inport_opt[0 + (__const__reg_bank_id_at_access_registers * 3)+:3];
			send_data_to_fu__msg = reg_file__rdata[1'd0 * 35+:35];
			send_data_to_xbar__msg = reg_file__rdata[1'd0 * 35+:35];
		end
		__tmpvar__access_registers_write_reg_from = inport_opt[32 + (__const__reg_bank_id_at_access_registers * 2)+:2];
		if (~reset & (__tmpvar__access_registers_write_reg_from > 2'd0)) begin
			wr_sel_data = inport_wdata[((2 + 2'd1) - __tmpvar__access_registers_write_reg_from) * 35+:35];
			if (inport_valid[(2 + 2'd1) - __tmpvar__access_registers_write_reg_from+:1] & outport_wr_rdy) begin
				wr_accept = 1'd1;
				if ((~wr_target_holds_token & ~wr_target_being_read) | (wr_target_being_read & inport_ctrl_proceed))
					wr_en = 1'd1;
				else
					skid_park = 1'd1;
			end
		end
		if (skid_commit) begin
			reg_file__waddr[1'd0 * 3+:3] = skid_idx;
			reg_file__wdata[1'd0 * 35+:35] = skid_data;
			reg_file__wen[1'd0+:1] = 1'd1;
		end
		else if (wr_en) begin
			reg_file__waddr[1'd0 * 3+:3] = inport_opt[20 + (__const__reg_bank_id_at_access_registers * 3)+:3];
			reg_file__wdata[1'd0 * 35+:35] = wr_sel_data;
			reg_file__wen[1'd0+:1] = 1'd1;
		end
	end
	always @(*) begin : update_send_val
		if (_sv2v_0)
			;
		send_data_to_fu__val = (~reset & read_towards_fu) & (read_token_valid | ~read_armed);
		send_data_to_xbar__val = (~reset & read_towards_xbar) & (read_token_valid | ~read_armed);
	end
	function automatic [2:0] sv2v_cast_3;
		input reg [2:0] inp;
		sv2v_cast_3 = inp;
	endfunction
	always @(*) begin : update_token_masks
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] r;
			for (r = 1'd0; r < __const__num_registers_at_update_token_masks; r = r + 1'd1)
				begin
					token_set_mask[sv2v_cast_3(r)] = (wr_en & (inport_opt[20 + (__const__reg_bank_id_at_update_token_masks * 3)+:3] == sv2v_cast_3(r))) | (skid_commit & (skid_idx == sv2v_cast_3(r)));
					token_clear_mask[sv2v_cast_3(r)] = ((inport_ctrl_proceed & (read_towards_fu | read_towards_xbar)) & (inport_opt[0 + (__const__reg_bank_id_at_update_token_masks * 3)+:3] == sv2v_cast_3(r))) & token_valid[sv2v_cast_3(r)];
				end
		end
	end
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	always @(*) begin : update_token_status
		if (_sv2v_0)
			;
		__tmpvar__update_token_status_read_towards = inport_opt[12 + (__const__reg_bank_id_at_update_token_status * 2)+:2];
		read_towards_fu = (__tmpvar__update_token_status_read_towards == sv2v_cast_2(__const__READ_TOWARDS_FU)) | (__tmpvar__update_token_status_read_towards == __const__READ_TOWARDS_BOTH);
		read_towards_xbar = (__tmpvar__update_token_status_read_towards == __const__READ_TOWARDS_ROUTING_XBAR) | (__tmpvar__update_token_status_read_towards == __const__READ_TOWARDS_BOTH);
		read_token_valid = 1'd0;
		read_armed = 1'd0;
		wr_target_holds_token = 1'd0;
		skid_target_holds_token = 1'd0;
		begin : sv2v_autoblock_2
			reg [31:0] r;
			for (r = 1'd0; r < __const__num_registers_at_update_token_status; r = r + 1'd1)
				begin
					if (inport_opt[0 + (__const__reg_bank_id_at_update_token_status * 3)+:3] == sv2v_cast_3(r)) begin
						read_token_valid = token_valid[sv2v_cast_3(r)];
						read_armed = armed[sv2v_cast_3(r)];
					end
					if (inport_opt[20 + (__const__reg_bank_id_at_update_token_status * 3)+:3] == sv2v_cast_3(r))
						wr_target_holds_token = token_valid[sv2v_cast_3(r)];
					if (skid_idx == sv2v_cast_3(r))
						skid_target_holds_token = token_valid[sv2v_cast_3(r)];
				end
		end
		wr_target_being_read = (read_towards_fu | read_towards_xbar) & (inport_opt[0 + (__const__reg_bank_id_at_update_token_status * 3)+:3] == inport_opt[20 + (__const__reg_bank_id_at_update_token_status * 3)+:3]);
		skid_target_being_read = (read_towards_fu | read_towards_xbar) & (inport_opt[0 + (__const__reg_bank_id_at_update_token_status * 3)+:3] == skid_idx);
		skid_commit = skid_valid & ((skid_target_being_read & inport_ctrl_proceed) | (~skid_target_being_read & ~skid_target_holds_token));
		outport_wr_rdy = (~wr_target_holds_token & ~wr_target_being_read) | (wr_target_being_read & ~skid_valid);
	end
	always @(posedge clk) begin : update_token_valid
		if (reset | clear) begin
			token_valid <= 8'd0;
			armed <= 8'd0;
			skid_valid <= 1'd0;
		end
		else begin
			token_valid <= (token_valid & ~token_clear_mask) | token_set_mask;
			armed <= armed | token_set_mask;
			if (skid_park) begin
				skid_valid <= 1'd1;
				skid_data <= wr_sel_data;
				skid_idx <= inport_opt[20 + (__const__reg_bank_id_at_update_token_valid * 3)+:3];
			end
			else if (skid_commit)
				skid_valid <= 1'd0;
		end
	end
	assign reg_file__clk = clk;
	assign reg_file__reset = reset;
	initial _sv2v_0 = 0;
endmodule
module RegisterClusterRTL__4cdc70707e060b34 (
	clear,
	clk,
	inport_ctrl_proceed,
	inport_opt,
	reset,
	recv_data_from_const__msg,
	recv_data_from_const__rdy,
	recv_data_from_const__val,
	recv_data_from_fu_crossbar__msg,
	recv_data_from_fu_crossbar__rdy,
	recv_data_from_fu_crossbar__val,
	recv_data_from_routing_crossbar__msg,
	recv_data_from_routing_crossbar__rdy,
	recv_data_from_routing_crossbar__val,
	send_data_to_fu__msg,
	send_data_to_fu__rdy,
	send_data_to_fu__val,
	send_data_to_routing_crossbar__msg,
	send_data_to_routing_crossbar__rdy,
	send_data_to_routing_crossbar__val
);
	reg _sv2v_0;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [0:0] inport_ctrl_proceed;
	input wire [110:0] inport_opt;
	input wire [0:0] reset;
	input wire [139:0] recv_data_from_const__msg;
	output reg [3:0] recv_data_from_const__rdy;
	input wire [3:0] recv_data_from_const__val;
	input wire [139:0] recv_data_from_fu_crossbar__msg;
	output reg [3:0] recv_data_from_fu_crossbar__rdy;
	input wire [3:0] recv_data_from_fu_crossbar__val;
	input wire [139:0] recv_data_from_routing_crossbar__msg;
	output reg [3:0] recv_data_from_routing_crossbar__rdy;
	input wire [3:0] recv_data_from_routing_crossbar__val;
	output reg [139:0] send_data_to_fu__msg;
	input wire [3:0] send_data_to_fu__rdy;
	output reg [3:0] send_data_to_fu__val;
	output wire [139:0] send_data_to_routing_crossbar__msg;
	input wire [3:0] send_data_to_routing_crossbar__rdy;
	output wire [3:0] send_data_to_routing_crossbar__val;
	localparam [2:0] __const__num_reg_banks_at_update_msgs_signals = 3'd4;
	localparam [0:0] __const__READ_TOWARDS_FU = 1'd1;
	localparam [1:0] __const__READ_TOWARDS_BOTH = 2'd3;
	localparam [0:0] __const__PORT_ROUTING_CROSSBAR = 1'd1;
	localparam [6:0] __const__OPT_NAH = 7'd1;
	localparam [0:0] __const__PORT_INDEX_ROUTING_CROSSBAR = 1'd0;
	localparam [1:0] __const__PORT_FU_CROSSBAR = 2'd2;
	localparam [1:0] __const__PORT_CONST = 2'd3;
	wire [0:0] reg_bank__clear [0:3];
	wire [0:0] reg_bank__clk [0:3];
	wire [0:0] reg_bank__inport_ctrl_proceed [0:3];
	wire [110:0] reg_bank__inport_opt [0:3];
	reg [2:0] reg_bank__inport_valid [0:3];
	wire [104:0] reg_bank__inport_wdata [0:3];
	wire [0:0] reg_bank__outport_wr_rdy [0:3];
	wire [0:0] reg_bank__reset [0:3];
	wire [34:0] reg_bank__send_data_to_fu__msg [0:3];
	reg [0:0] reg_bank__send_data_to_fu__rdy [0:3];
	wire [0:0] reg_bank__send_data_to_fu__val [0:3];
	wire [34:0] reg_bank__send_data_to_xbar__msg [0:3];
	wire [0:0] reg_bank__send_data_to_xbar__rdy [0:3];
	wire [0:0] reg_bank__send_data_to_xbar__val [0:3];
	RegisterBankRTL__493e7203765a09b1 reg_bank__0(
		.clear(reg_bank__clear[0]),
		.clk(reg_bank__clk[0]),
		.inport_ctrl_proceed(reg_bank__inport_ctrl_proceed[0]),
		.inport_opt(reg_bank__inport_opt[0]),
		.inport_valid(reg_bank__inport_valid[0]),
		.inport_wdata(reg_bank__inport_wdata[0]),
		.outport_wr_rdy(reg_bank__outport_wr_rdy[0]),
		.reset(reg_bank__reset[0]),
		.send_data_to_fu__msg(reg_bank__send_data_to_fu__msg[0]),
		.send_data_to_fu__rdy(reg_bank__send_data_to_fu__rdy[0]),
		.send_data_to_fu__val(reg_bank__send_data_to_fu__val[0]),
		.send_data_to_xbar__msg(reg_bank__send_data_to_xbar__msg[0]),
		.send_data_to_xbar__rdy(reg_bank__send_data_to_xbar__rdy[0]),
		.send_data_to_xbar__val(reg_bank__send_data_to_xbar__val[0])
	);
	RegisterBankRTL__3596ca3621cddc37 reg_bank__1(
		.clear(reg_bank__clear[1]),
		.clk(reg_bank__clk[1]),
		.inport_ctrl_proceed(reg_bank__inport_ctrl_proceed[1]),
		.inport_opt(reg_bank__inport_opt[1]),
		.inport_valid(reg_bank__inport_valid[1]),
		.inport_wdata(reg_bank__inport_wdata[1]),
		.outport_wr_rdy(reg_bank__outport_wr_rdy[1]),
		.reset(reg_bank__reset[1]),
		.send_data_to_fu__msg(reg_bank__send_data_to_fu__msg[1]),
		.send_data_to_fu__rdy(reg_bank__send_data_to_fu__rdy[1]),
		.send_data_to_fu__val(reg_bank__send_data_to_fu__val[1]),
		.send_data_to_xbar__msg(reg_bank__send_data_to_xbar__msg[1]),
		.send_data_to_xbar__rdy(reg_bank__send_data_to_xbar__rdy[1]),
		.send_data_to_xbar__val(reg_bank__send_data_to_xbar__val[1])
	);
	RegisterBankRTL__c9691accc64d09b9 reg_bank__2(
		.clear(reg_bank__clear[2]),
		.clk(reg_bank__clk[2]),
		.inport_ctrl_proceed(reg_bank__inport_ctrl_proceed[2]),
		.inport_opt(reg_bank__inport_opt[2]),
		.inport_valid(reg_bank__inport_valid[2]),
		.inport_wdata(reg_bank__inport_wdata[2]),
		.outport_wr_rdy(reg_bank__outport_wr_rdy[2]),
		.reset(reg_bank__reset[2]),
		.send_data_to_fu__msg(reg_bank__send_data_to_fu__msg[2]),
		.send_data_to_fu__rdy(reg_bank__send_data_to_fu__rdy[2]),
		.send_data_to_fu__val(reg_bank__send_data_to_fu__val[2]),
		.send_data_to_xbar__msg(reg_bank__send_data_to_xbar__msg[2]),
		.send_data_to_xbar__rdy(reg_bank__send_data_to_xbar__rdy[2]),
		.send_data_to_xbar__val(reg_bank__send_data_to_xbar__val[2])
	);
	RegisterBankRTL__9a44c4bf7f9388e0 reg_bank__3(
		.clear(reg_bank__clear[3]),
		.clk(reg_bank__clk[3]),
		.inport_ctrl_proceed(reg_bank__inport_ctrl_proceed[3]),
		.inport_opt(reg_bank__inport_opt[3]),
		.inport_valid(reg_bank__inport_valid[3]),
		.inport_wdata(reg_bank__inport_wdata[3]),
		.outport_wr_rdy(reg_bank__outport_wr_rdy[3]),
		.reset(reg_bank__reset[3]),
		.send_data_to_fu__msg(reg_bank__send_data_to_fu__msg[3]),
		.send_data_to_fu__rdy(reg_bank__send_data_to_fu__rdy[3]),
		.send_data_to_fu__val(reg_bank__send_data_to_fu__val[3]),
		.send_data_to_xbar__msg(reg_bank__send_data_to_xbar__msg[3]),
		.send_data_to_xbar__rdy(reg_bank__send_data_to_xbar__rdy[3]),
		.send_data_to_xbar__val(reg_bank__send_data_to_xbar__val[3])
	);
	reg [0:0] __tmpvar__update_msgs_signals_lane_used;
	reg [1:0] __tmpvar__update_msgs_signals_read_towards;
	reg [0:0] __tmpvar__update_msgs_signals_reg_towards_fu;
	reg [0:0] __tmpvar__update_msgs_signals_routing_write_rdy;
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	function automatic [2:0] sv2v_cast_3;
		input reg [2:0] inp;
		sv2v_cast_3 = inp;
	endfunction
	always @(*) begin : update_msgs_signals
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_reg_banks_at_update_msgs_signals; i = i + 1'd1)
				begin
					send_data_to_fu__msg[(3 - sv2v_cast_2(i)) * 35+:35] = 35'h000000000;
					recv_data_from_routing_crossbar__rdy[3 - sv2v_cast_2(i)+:1] = 1'd0;
					recv_data_from_fu_crossbar__rdy[3 - sv2v_cast_2(i)+:1] = 1'd0;
					recv_data_from_const__rdy[3 - sv2v_cast_2(i)+:1] = 1'd0;
					send_data_to_fu__val[3 - sv2v_cast_2(i)+:1] = 1'd0;
				end
		end
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_reg_banks_at_update_msgs_signals; i = i + 1'd1)
				begin
					__tmpvar__update_msgs_signals_lane_used = 1'd0;
					begin : sv2v_autoblock_3
						reg [31:0] j;
						for (j = 1'd0; j < __const__num_reg_banks_at_update_msgs_signals; j = j + 1'd1)
							__tmpvar__update_msgs_signals_lane_used = __tmpvar__update_msgs_signals_lane_used | (inport_opt[92 + (sv2v_cast_2(j) * 3)+:3] == (sv2v_cast_3(i) + 3'd1));
					end
					__tmpvar__update_msgs_signals_read_towards = inport_opt[12 + (sv2v_cast_2(i) * 2)+:2];
					__tmpvar__update_msgs_signals_reg_towards_fu = (__tmpvar__update_msgs_signals_read_towards == sv2v_cast_2(__const__READ_TOWARDS_FU)) | (__tmpvar__update_msgs_signals_read_towards == __const__READ_TOWARDS_BOTH);
					__tmpvar__update_msgs_signals_routing_write_rdy = (inport_opt[32 + (sv2v_cast_2(i) * 2)+:2] != sv2v_cast_2(__const__PORT_ROUTING_CROSSBAR)) | reg_bank__outport_wr_rdy[sv2v_cast_2(i)];
					if (reg_bank__send_data_to_fu__val[sv2v_cast_2(i)])
						send_data_to_fu__msg[(3 - sv2v_cast_2(i)) * 35+:35] = reg_bank__send_data_to_fu__msg[sv2v_cast_2(i)];
					else if (recv_data_from_routing_crossbar__val[3 - sv2v_cast_2(i)+:1])
						send_data_to_fu__msg[(3 - sv2v_cast_2(i)) * 35+:35] = recv_data_from_routing_crossbar__msg[(3 - sv2v_cast_2(i)) * 35+:35];
					send_data_to_fu__val[3 - sv2v_cast_2(i)+:1] = (recv_data_from_routing_crossbar__val[3 - sv2v_cast_2(i)+:1] & __tmpvar__update_msgs_signals_routing_write_rdy) | reg_bank__send_data_to_fu__val[sv2v_cast_2(i)];
					reg_bank__send_data_to_fu__rdy[sv2v_cast_2(i)] = send_data_to_fu__rdy[3 - sv2v_cast_2(i)+:1];
					recv_data_from_routing_crossbar__rdy[3 - sv2v_cast_2(i)+:1] = __tmpvar__update_msgs_signals_routing_write_rdy & ((((inport_opt[110-:7] == __const__OPT_NAH) | ~__tmpvar__update_msgs_signals_lane_used) | __tmpvar__update_msgs_signals_reg_towards_fu) | send_data_to_fu__rdy[3 - sv2v_cast_2(i)+:1]);
					reg_bank__inport_valid[sv2v_cast_2(i)][2 - sv2v_cast_2(__const__PORT_INDEX_ROUTING_CROSSBAR)+:1] = recv_data_from_routing_crossbar__val[3 - sv2v_cast_2(i)+:1] & recv_data_from_routing_crossbar__rdy[3 - sv2v_cast_2(i)+:1];
					recv_data_from_fu_crossbar__rdy[3 - sv2v_cast_2(i)+:1] = (inport_opt[32 + (sv2v_cast_2(i) * 2)+:2] != __const__PORT_FU_CROSSBAR) | reg_bank__outport_wr_rdy[sv2v_cast_2(i)];
					recv_data_from_const__rdy[3 - sv2v_cast_2(i)+:1] = (inport_opt[32 + (sv2v_cast_2(i) * 2)+:2] != __const__PORT_CONST) | reg_bank__outport_wr_rdy[sv2v_cast_2(i)];
				end
		end
	end
	assign reg_bank__clk[0] = clk;
	assign reg_bank__reset[0] = reset;
	assign reg_bank__clk[1] = clk;
	assign reg_bank__reset[1] = reset;
	assign reg_bank__clk[2] = clk;
	assign reg_bank__reset[2] = reset;
	assign reg_bank__clk[3] = clk;
	assign reg_bank__reset[3] = reset;
	assign reg_bank__inport_opt[0] = inport_opt;
	assign reg_bank__inport_ctrl_proceed[0] = inport_ctrl_proceed;
	assign reg_bank__clear[0] = clear;
	assign reg_bank__inport_wdata[0][70+:35] = recv_data_from_routing_crossbar__msg[105+:35];
	assign reg_bank__inport_wdata[0][35+:35] = recv_data_from_fu_crossbar__msg[105+:35];
	assign reg_bank__inport_wdata[0][0+:35] = recv_data_from_const__msg[105+:35];
	wire [1:1] sv2v_tmp_823C6;
	assign sv2v_tmp_823C6 = recv_data_from_fu_crossbar__val[3+:1];
	always @(*) reg_bank__inport_valid[0][1+:1] = sv2v_tmp_823C6;
	wire [1:1] sv2v_tmp_D3757;
	assign sv2v_tmp_D3757 = recv_data_from_const__val[3+:1];
	always @(*) reg_bank__inport_valid[0][0+:1] = sv2v_tmp_D3757;
	assign send_data_to_routing_crossbar__msg[105+:35] = reg_bank__send_data_to_xbar__msg[0];
	assign reg_bank__send_data_to_xbar__rdy[0] = send_data_to_routing_crossbar__rdy[3+:1];
	assign send_data_to_routing_crossbar__val[3+:1] = reg_bank__send_data_to_xbar__val[0];
	assign reg_bank__inport_opt[1] = inport_opt;
	assign reg_bank__inport_ctrl_proceed[1] = inport_ctrl_proceed;
	assign reg_bank__clear[1] = clear;
	assign reg_bank__inport_wdata[1][70+:35] = recv_data_from_routing_crossbar__msg[70+:35];
	assign reg_bank__inport_wdata[1][35+:35] = recv_data_from_fu_crossbar__msg[70+:35];
	assign reg_bank__inport_wdata[1][0+:35] = recv_data_from_const__msg[70+:35];
	wire [1:1] sv2v_tmp_2BFD6;
	assign sv2v_tmp_2BFD6 = recv_data_from_fu_crossbar__val[2+:1];
	always @(*) reg_bank__inport_valid[1][1+:1] = sv2v_tmp_2BFD6;
	wire [1:1] sv2v_tmp_E1207;
	assign sv2v_tmp_E1207 = recv_data_from_const__val[2+:1];
	always @(*) reg_bank__inport_valid[1][0+:1] = sv2v_tmp_E1207;
	assign send_data_to_routing_crossbar__msg[70+:35] = reg_bank__send_data_to_xbar__msg[1];
	assign reg_bank__send_data_to_xbar__rdy[1] = send_data_to_routing_crossbar__rdy[2+:1];
	assign send_data_to_routing_crossbar__val[2+:1] = reg_bank__send_data_to_xbar__val[1];
	assign reg_bank__inport_opt[2] = inport_opt;
	assign reg_bank__inport_ctrl_proceed[2] = inport_ctrl_proceed;
	assign reg_bank__clear[2] = clear;
	assign reg_bank__inport_wdata[2][70+:35] = recv_data_from_routing_crossbar__msg[35+:35];
	assign reg_bank__inport_wdata[2][35+:35] = recv_data_from_fu_crossbar__msg[35+:35];
	assign reg_bank__inport_wdata[2][0+:35] = recv_data_from_const__msg[35+:35];
	wire [1:1] sv2v_tmp_D6726;
	assign sv2v_tmp_D6726 = recv_data_from_fu_crossbar__val[1+:1];
	always @(*) reg_bank__inport_valid[2][1+:1] = sv2v_tmp_D6726;
	wire [1:1] sv2v_tmp_DACB7;
	assign sv2v_tmp_DACB7 = recv_data_from_const__val[1+:1];
	always @(*) reg_bank__inport_valid[2][0+:1] = sv2v_tmp_DACB7;
	assign send_data_to_routing_crossbar__msg[35+:35] = reg_bank__send_data_to_xbar__msg[2];
	assign reg_bank__send_data_to_xbar__rdy[2] = send_data_to_routing_crossbar__rdy[1+:1];
	assign send_data_to_routing_crossbar__val[1+:1] = reg_bank__send_data_to_xbar__val[2];
	assign reg_bank__inport_opt[3] = inport_opt;
	assign reg_bank__inport_ctrl_proceed[3] = inport_ctrl_proceed;
	assign reg_bank__clear[3] = clear;
	assign reg_bank__inport_wdata[3][70+:35] = recv_data_from_routing_crossbar__msg[0+:35];
	assign reg_bank__inport_wdata[3][35+:35] = recv_data_from_fu_crossbar__msg[0+:35];
	assign reg_bank__inport_wdata[3][0+:35] = recv_data_from_const__msg[0+:35];
	wire [1:1] sv2v_tmp_D8CB6;
	assign sv2v_tmp_D8CB6 = recv_data_from_fu_crossbar__val[0+:1];
	always @(*) reg_bank__inport_valid[3][1+:1] = sv2v_tmp_D8CB6;
	wire [1:1] sv2v_tmp_2D667;
	assign sv2v_tmp_2D667 = recv_data_from_const__val[0+:1];
	always @(*) reg_bank__inport_valid[3][0+:1] = sv2v_tmp_2D667;
	assign send_data_to_routing_crossbar__msg[0+:35] = reg_bank__send_data_to_xbar__msg[3];
	assign reg_bank__send_data_to_xbar__rdy[3] = send_data_to_routing_crossbar__rdy[0+:1];
	assign send_data_to_routing_crossbar__val[0+:1] = reg_bank__send_data_to_xbar__val[3];
	initial _sv2v_0 = 0;
endmodule
module CrossbarRTL__f9cc749ddff26d17 (
	cgra_id,
	clear,
	clk,
	compute_done,
	crossbar_id,
	crossbar_outport,
	ctrl_addr_inport,
	prologue_count_inport,
	reset,
	tile_id,
	recv_data__msg,
	recv_data__rdy,
	recv_data__val,
	recv_opt__msg,
	recv_opt__rdy,
	recv_opt__val,
	send_data__msg,
	send_data__rdy,
	send_data__val
);
	reg _sv2v_0;
	input wire [0:0] cgra_id;
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [0:0] compute_done;
	input wire [0:0] crossbar_id;
	input wire [31:0] crossbar_outport;
	input wire [5:0] ctrl_addr_inport;
	input wire [1151:0] prologue_count_inport;
	input wire [0:0] reset;
	input wire [4:0] tile_id;
	input wire [279:0] recv_data__msg;
	output reg [7:0] recv_data__rdy;
	input wire [7:0] recv_data__val;
	input wire [110:0] recv_opt__msg;
	output reg [0:0] recv_opt__rdy;
	input wire [0:0] recv_opt__val;
	output reg [279:0] send_data__msg;
	input wire [7:0] send_data__rdy;
	output reg [7:0] send_data__val;
	localparam [3:0] __const__num_inports_at_update_signal = 4'd8;
	localparam [3:0] __const__num_outports_at_update_signal = 4'd8;
	localparam [6:0] __const__OPT_START = 7'd0;
	localparam [5:0] __const__ctrl_mem_size_at_update_prologue_counter = 6'd48;
	localparam [3:0] __const__num_inports_at_update_prologue_counter = 4'd8;
	localparam [5:0] __const__ctrl_mem_size_at_update_prologue_counter_next = 6'd48;
	localparam [3:0] __const__num_inports_at_update_prologue_counter_next = 4'd8;
	localparam [3:0] __const__num_outports_at_update_prologue_counter_next = 4'd8;
	localparam [3:0] __const__num_outports_at_update_send_accepted_next = 4'd8;
	localparam [3:0] __const__num_outports_at_update_prologue_allowing_vector = 4'd8;
	localparam [3:0] __const__num_outports_at_update_prologue_or_valid_vector = 4'd8;
	localparam [3:0] __const__num_outports_at_update_in_dir_vector = 4'd8;
	localparam [3:0] __const__num_outports_at_update_rdy_vector = 4'd8;
	localparam [2:0] __const__outport_towards_local_base_id_at_update_rdy_vector = 3'd4;
	localparam [3:0] __const__num_outports_at_update_valid_vector = 4'd8;
	localparam [3:0] __const__num_inports_at_update_recv_required_vector = 4'd8;
	localparam [3:0] __const__num_outports_at_update_recv_required_vector = 4'd8;
	localparam [3:0] __const__num_outports_at_update_send_required_vector = 4'd8;
	reg [0:0] all_send_accepted;
	reg [7:0] during_prologue_allowing_vector;
	reg [3:0] in_dir [0:7];
	reg [2:0] in_dir_local [0:7];
	wire [2:0] prologue_count_wire [0:47][0:7];
	reg [2:0] prologue_counter [0:47][0:7];
	reg [2:0] prologue_counter_next [0:47][0:7];
	wire [34:0] recv_data_msg [0:7];
	wire [0:0] recv_data_val [0:7];
	reg [7:0] recv_required_vector;
	reg [7:0] recv_valid_excluding_prologue_vector;
	reg [7:0] recv_valid_or_during_prologue_allowing_vector;
	reg [7:0] send_accepted;
	reg [7:0] send_accepted_next;
	reg [7:0] send_rdy_or_during_prologue_vector;
	reg [7:0] send_required_vector;
	function automatic [2:0] sv2v_cast_3;
		input reg [2:0] inp;
		sv2v_cast_3 = inp;
	endfunction
	always @(*) begin : update_in_dir_vector
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_in_dir_vector; i = i + 1'd1)
				begin
					in_dir[sv2v_cast_3(i)] = 4'd0;
					in_dir_local[sv2v_cast_3(i)] = 3'd0;
				end
		end
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_in_dir_vector; i = i + 1'd1)
				begin
					in_dir[sv2v_cast_3(i)] = crossbar_outport[(7 - sv2v_cast_3(i)) * 4+:4];
					if (in_dir[sv2v_cast_3(i)] > 4'd0)
						in_dir_local[sv2v_cast_3(i)] = sv2v_cast_3(in_dir[sv2v_cast_3(i)] - 4'd1);
				end
		end
	end
	always @(*) begin : update_prologue_allowing_vector
		if (_sv2v_0)
			;
		during_prologue_allowing_vector = 8'd0;
		begin : sv2v_autoblock_3
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_prologue_allowing_vector; i = i + 1'd1)
				if (in_dir[sv2v_cast_3(i)] > 4'd0)
					during_prologue_allowing_vector[sv2v_cast_3(i)] = prologue_counter[ctrl_addr_inport][in_dir_local[sv2v_cast_3(i)]] < prologue_count_wire[ctrl_addr_inport][in_dir_local[sv2v_cast_3(i)]];
				else
					during_prologue_allowing_vector[sv2v_cast_3(i)] = 1'd0;
		end
	end
	function automatic [5:0] sv2v_cast_6;
		input reg [5:0] inp;
		sv2v_cast_6 = inp;
	endfunction
	always @(*) begin : update_prologue_counter_next
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_4
			reg [31:0] addr;
			for (addr = 1'd0; addr < __const__ctrl_mem_size_at_update_prologue_counter_next; addr = addr + 1'd1)
				begin : sv2v_autoblock_5
					reg [31:0] i;
					for (i = 1'd0; i < __const__num_inports_at_update_prologue_counter_next; i = i + 1'd1)
						begin
							prologue_counter_next[sv2v_cast_6(addr)][sv2v_cast_3(i)] = prologue_counter[sv2v_cast_6(addr)][sv2v_cast_3(i)];
							begin : sv2v_autoblock_6
								reg [31:0] j;
								for (j = 1'd0; j < __const__num_outports_at_update_prologue_counter_next; j = j + 1'd1)
									if ((((recv_opt__rdy & (in_dir[sv2v_cast_3(j)] > 4'd0)) & (in_dir_local[sv2v_cast_3(j)] == sv2v_cast_3(i))) & (sv2v_cast_6(addr) == ctrl_addr_inport)) & (prologue_counter[sv2v_cast_6(addr)][sv2v_cast_3(i)] < prologue_count_wire[sv2v_cast_6(addr)][sv2v_cast_3(i)]))
										prologue_counter_next[sv2v_cast_6(addr)][sv2v_cast_3(i)] = prologue_counter[sv2v_cast_6(addr)][sv2v_cast_3(i)] + 3'd1;
							end
						end
				end
		end
	end
	always @(*) begin : update_prologue_or_valid_vector
		if (_sv2v_0)
			;
		recv_valid_or_during_prologue_allowing_vector = 8'd0;
		begin : sv2v_autoblock_7
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_prologue_or_valid_vector; i = i + 1'd1)
				recv_valid_or_during_prologue_allowing_vector[sv2v_cast_3(i)] = recv_valid_excluding_prologue_vector[sv2v_cast_3(i)] | during_prologue_allowing_vector[sv2v_cast_3(i)];
		end
	end
	always @(*) begin : update_rdy_vector
		if (_sv2v_0)
			;
		send_rdy_or_during_prologue_vector = 8'd0;
		begin : sv2v_autoblock_8
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_rdy_vector; i = i + 1'd1)
				if ((in_dir[sv2v_cast_3(i)] > 4'd0) & (~compute_done | (sv2v_cast_3(i) < __const__outport_towards_local_base_id_at_update_rdy_vector)))
					send_rdy_or_during_prologue_vector[sv2v_cast_3(i)] = send_data__rdy[7 - sv2v_cast_3(i)+:1] | during_prologue_allowing_vector[sv2v_cast_3(i)];
				else
					send_rdy_or_during_prologue_vector[sv2v_cast_3(i)] = 1'd1;
		end
	end
	always @(*) begin : update_recv_required_vector
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_9
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_update_recv_required_vector; i = i + 1'd1)
				recv_required_vector[sv2v_cast_3(i)] = 1'd0;
		end
		begin : sv2v_autoblock_10
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_recv_required_vector; i = i + 1'd1)
				if ((in_dir[sv2v_cast_3(i)] > 4'd0) & ~during_prologue_allowing_vector[sv2v_cast_3(i)])
					recv_required_vector[in_dir_local[sv2v_cast_3(i)]] = 1'd1;
		end
	end
	always @(*) begin : update_send_accepted_next
		if (_sv2v_0)
			;
		send_accepted_next = send_accepted;
		if (recv_opt__val & (recv_opt__msg[110-:7] != __const__OPT_START)) begin
			begin : sv2v_autoblock_11
				reg [31:0] i;
				for (i = 1'd0; i < __const__num_outports_at_update_send_accepted_next; i = i + 1'd1)
					if (((send_required_vector[sv2v_cast_3(i)] & ~send_accepted[sv2v_cast_3(i)]) & send_data__rdy[7 - sv2v_cast_3(i)+:1]) & send_data__val[7 - sv2v_cast_3(i)+:1])
						send_accepted_next[sv2v_cast_3(i)] = 1'd1;
			end
			if (recv_opt__rdy)
				send_accepted_next = 8'd0;
		end
		else
			send_accepted_next = 8'd0;
	end
	always @(*) begin : update_send_required_vector
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_12
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_send_required_vector; i = i + 1'd1)
				send_required_vector[sv2v_cast_3(i)] = 1'd0;
		end
		begin : sv2v_autoblock_13
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_send_required_vector; i = i + 1'd1)
				if ((in_dir[sv2v_cast_3(i)] > 4'd0) & ~during_prologue_allowing_vector[sv2v_cast_3(i)])
					send_required_vector[sv2v_cast_3(i)] = 1'd1;
		end
	end
	always @(*) begin : update_signal
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_14
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_inports_at_update_signal; i = i + 1'd1)
				recv_data__rdy[7 - sv2v_cast_3(i)+:1] = 1'd0;
		end
		begin : sv2v_autoblock_15
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_signal; i = i + 1'd1)
				begin
					send_data__val[7 - sv2v_cast_3(i)+:1] = 1'd0;
					send_data__msg[(7 - sv2v_cast_3(i)) * 35+:35] = 35'h000000000;
				end
		end
		recv_opt__rdy = 1'd0;
		all_send_accepted = 1'd0;
		if (recv_opt__val & (recv_opt__msg[110-:7] != __const__OPT_START)) begin
			all_send_accepted = 1'd1;
			begin : sv2v_autoblock_16
				reg [31:0] i;
				for (i = 1'd0; i < __const__num_outports_at_update_signal; i = i + 1'd1)
					if ((send_required_vector[sv2v_cast_3(i)] & ~send_accepted[sv2v_cast_3(i)]) & ~send_rdy_or_during_prologue_vector[sv2v_cast_3(i)])
						all_send_accepted = 1'd0;
			end
			begin : sv2v_autoblock_17
				reg [31:0] i;
				for (i = 1'd0; i < __const__num_inports_at_update_signal; i = i + 1'd1)
					recv_data__rdy[7 - sv2v_cast_3(i)+:1] = (&recv_valid_or_during_prologue_allowing_vector & all_send_accepted) & recv_required_vector[sv2v_cast_3(i)];
			end
			begin : sv2v_autoblock_18
				reg [31:0] i;
				for (i = 1'd0; i < __const__num_outports_at_update_signal; i = i + 1'd1)
					begin
						send_data__val[7 - sv2v_cast_3(i)+:1] = (&recv_valid_or_during_prologue_allowing_vector & send_required_vector[sv2v_cast_3(i)]) & ~send_accepted[sv2v_cast_3(i)];
						if ((&recv_valid_or_during_prologue_allowing_vector & send_required_vector[sv2v_cast_3(i)]) & ~send_accepted[sv2v_cast_3(i)]) begin
							send_data__msg[((7 - sv2v_cast_3(i)) * 35) + 34-:32] = recv_data_msg[in_dir_local[sv2v_cast_3(i)]][34-:32];
							send_data__msg[((7 - sv2v_cast_3(i)) * 35) + 2] = recv_data_msg[in_dir_local[sv2v_cast_3(i)]][2];
						end
					end
			end
			recv_opt__rdy = all_send_accepted & (&recv_valid_or_during_prologue_allowing_vector);
		end
	end
	always @(*) begin : update_valid_vector
		if (_sv2v_0)
			;
		recv_valid_excluding_prologue_vector = 8'd0;
		begin : sv2v_autoblock_19
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_outports_at_update_valid_vector; i = i + 1'd1)
				if (in_dir[sv2v_cast_3(i)] > 4'd0)
					recv_valid_excluding_prologue_vector[sv2v_cast_3(i)] = recv_data_val[in_dir_local[sv2v_cast_3(i)]] & ~during_prologue_allowing_vector[sv2v_cast_3(i)];
				else
					recv_valid_excluding_prologue_vector[sv2v_cast_3(i)] = 1'd1;
		end
	end
	always @(posedge clk) begin : update_prologue_counter
		if (reset | clear) begin
			begin : sv2v_autoblock_20
				reg [31:0] addr;
				for (addr = 1'd0; addr < __const__ctrl_mem_size_at_update_prologue_counter; addr = addr + 1'd1)
					begin : sv2v_autoblock_21
						reg [31:0] i;
						for (i = 1'd0; i < __const__num_inports_at_update_prologue_counter; i = i + 1'd1)
							prologue_counter[sv2v_cast_6(addr)][sv2v_cast_3(i)] <= 3'd0;
					end
			end
			send_accepted <= 8'd0;
		end
		else begin
			begin : sv2v_autoblock_22
				reg [31:0] addr;
				for (addr = 1'd0; addr < __const__ctrl_mem_size_at_update_prologue_counter; addr = addr + 1'd1)
					begin : sv2v_autoblock_23
						reg [31:0] i;
						for (i = 1'd0; i < __const__num_inports_at_update_prologue_counter; i = i + 1'd1)
							prologue_counter[sv2v_cast_6(addr)][sv2v_cast_3(i)] <= prologue_counter_next[sv2v_cast_6(addr)][sv2v_cast_3(i)];
					end
			end
			send_accepted <= send_accepted_next;
		end
	end
	assign recv_data_msg[0] = recv_data__msg[245+:35];
	assign recv_data_val[0] = recv_data__val[7+:1];
	assign recv_data_msg[1] = recv_data__msg[210+:35];
	assign recv_data_val[1] = recv_data__val[6+:1];
	assign recv_data_msg[2] = recv_data__msg[175+:35];
	assign recv_data_val[2] = recv_data__val[5+:1];
	assign recv_data_msg[3] = recv_data__msg[140+:35];
	assign recv_data_val[3] = recv_data__val[4+:1];
	assign recv_data_msg[4] = recv_data__msg[105+:35];
	assign recv_data_val[4] = recv_data__val[3+:1];
	assign recv_data_msg[5] = recv_data__msg[70+:35];
	assign recv_data_val[5] = recv_data__val[2+:1];
	assign recv_data_msg[6] = recv_data__msg[35+:35];
	assign recv_data_val[6] = recv_data__val[1+:1];
	assign recv_data_msg[7] = recv_data__msg[0+:35];
	assign recv_data_val[7] = recv_data__val[0+:1];
	assign prologue_count_wire[0][0] = prologue_count_inport[1149+:3];
	assign prologue_count_wire[0][1] = prologue_count_inport[1146+:3];
	assign prologue_count_wire[0][2] = prologue_count_inport[1143+:3];
	assign prologue_count_wire[0][3] = prologue_count_inport[1140+:3];
	assign prologue_count_wire[0][4] = prologue_count_inport[1137+:3];
	assign prologue_count_wire[0][5] = prologue_count_inport[1134+:3];
	assign prologue_count_wire[0][6] = prologue_count_inport[1131+:3];
	assign prologue_count_wire[0][7] = prologue_count_inport[1128+:3];
	assign prologue_count_wire[1][0] = prologue_count_inport[1125+:3];
	assign prologue_count_wire[1][1] = prologue_count_inport[1122+:3];
	assign prologue_count_wire[1][2] = prologue_count_inport[1119+:3];
	assign prologue_count_wire[1][3] = prologue_count_inport[1116+:3];
	assign prologue_count_wire[1][4] = prologue_count_inport[1113+:3];
	assign prologue_count_wire[1][5] = prologue_count_inport[1110+:3];
	assign prologue_count_wire[1][6] = prologue_count_inport[1107+:3];
	assign prologue_count_wire[1][7] = prologue_count_inport[1104+:3];
	assign prologue_count_wire[2][0] = prologue_count_inport[1101+:3];
	assign prologue_count_wire[2][1] = prologue_count_inport[1098+:3];
	assign prologue_count_wire[2][2] = prologue_count_inport[1095+:3];
	assign prologue_count_wire[2][3] = prologue_count_inport[1092+:3];
	assign prologue_count_wire[2][4] = prologue_count_inport[1089+:3];
	assign prologue_count_wire[2][5] = prologue_count_inport[1086+:3];
	assign prologue_count_wire[2][6] = prologue_count_inport[1083+:3];
	assign prologue_count_wire[2][7] = prologue_count_inport[1080+:3];
	assign prologue_count_wire[3][0] = prologue_count_inport[1077+:3];
	assign prologue_count_wire[3][1] = prologue_count_inport[1074+:3];
	assign prologue_count_wire[3][2] = prologue_count_inport[1071+:3];
	assign prologue_count_wire[3][3] = prologue_count_inport[1068+:3];
	assign prologue_count_wire[3][4] = prologue_count_inport[1065+:3];
	assign prologue_count_wire[3][5] = prologue_count_inport[1062+:3];
	assign prologue_count_wire[3][6] = prologue_count_inport[1059+:3];
	assign prologue_count_wire[3][7] = prologue_count_inport[1056+:3];
	assign prologue_count_wire[4][0] = prologue_count_inport[1053+:3];
	assign prologue_count_wire[4][1] = prologue_count_inport[1050+:3];
	assign prologue_count_wire[4][2] = prologue_count_inport[1047+:3];
	assign prologue_count_wire[4][3] = prologue_count_inport[1044+:3];
	assign prologue_count_wire[4][4] = prologue_count_inport[1041+:3];
	assign prologue_count_wire[4][5] = prologue_count_inport[1038+:3];
	assign prologue_count_wire[4][6] = prologue_count_inport[1035+:3];
	assign prologue_count_wire[4][7] = prologue_count_inport[1032+:3];
	assign prologue_count_wire[5][0] = prologue_count_inport[1029+:3];
	assign prologue_count_wire[5][1] = prologue_count_inport[1026+:3];
	assign prologue_count_wire[5][2] = prologue_count_inport[1023+:3];
	assign prologue_count_wire[5][3] = prologue_count_inport[1020+:3];
	assign prologue_count_wire[5][4] = prologue_count_inport[1017+:3];
	assign prologue_count_wire[5][5] = prologue_count_inport[1014+:3];
	assign prologue_count_wire[5][6] = prologue_count_inport[1011+:3];
	assign prologue_count_wire[5][7] = prologue_count_inport[1008+:3];
	assign prologue_count_wire[6][0] = prologue_count_inport[1005+:3];
	assign prologue_count_wire[6][1] = prologue_count_inport[1002+:3];
	assign prologue_count_wire[6][2] = prologue_count_inport[999+:3];
	assign prologue_count_wire[6][3] = prologue_count_inport[996+:3];
	assign prologue_count_wire[6][4] = prologue_count_inport[993+:3];
	assign prologue_count_wire[6][5] = prologue_count_inport[990+:3];
	assign prologue_count_wire[6][6] = prologue_count_inport[987+:3];
	assign prologue_count_wire[6][7] = prologue_count_inport[984+:3];
	assign prologue_count_wire[7][0] = prologue_count_inport[981+:3];
	assign prologue_count_wire[7][1] = prologue_count_inport[978+:3];
	assign prologue_count_wire[7][2] = prologue_count_inport[975+:3];
	assign prologue_count_wire[7][3] = prologue_count_inport[972+:3];
	assign prologue_count_wire[7][4] = prologue_count_inport[969+:3];
	assign prologue_count_wire[7][5] = prologue_count_inport[966+:3];
	assign prologue_count_wire[7][6] = prologue_count_inport[963+:3];
	assign prologue_count_wire[7][7] = prologue_count_inport[960+:3];
	assign prologue_count_wire[8][0] = prologue_count_inport[957+:3];
	assign prologue_count_wire[8][1] = prologue_count_inport[954+:3];
	assign prologue_count_wire[8][2] = prologue_count_inport[951+:3];
	assign prologue_count_wire[8][3] = prologue_count_inport[948+:3];
	assign prologue_count_wire[8][4] = prologue_count_inport[945+:3];
	assign prologue_count_wire[8][5] = prologue_count_inport[942+:3];
	assign prologue_count_wire[8][6] = prologue_count_inport[939+:3];
	assign prologue_count_wire[8][7] = prologue_count_inport[936+:3];
	assign prologue_count_wire[9][0] = prologue_count_inport[933+:3];
	assign prologue_count_wire[9][1] = prologue_count_inport[930+:3];
	assign prologue_count_wire[9][2] = prologue_count_inport[927+:3];
	assign prologue_count_wire[9][3] = prologue_count_inport[924+:3];
	assign prologue_count_wire[9][4] = prologue_count_inport[921+:3];
	assign prologue_count_wire[9][5] = prologue_count_inport[918+:3];
	assign prologue_count_wire[9][6] = prologue_count_inport[915+:3];
	assign prologue_count_wire[9][7] = prologue_count_inport[912+:3];
	assign prologue_count_wire[10][0] = prologue_count_inport[909+:3];
	assign prologue_count_wire[10][1] = prologue_count_inport[906+:3];
	assign prologue_count_wire[10][2] = prologue_count_inport[903+:3];
	assign prologue_count_wire[10][3] = prologue_count_inport[900+:3];
	assign prologue_count_wire[10][4] = prologue_count_inport[897+:3];
	assign prologue_count_wire[10][5] = prologue_count_inport[894+:3];
	assign prologue_count_wire[10][6] = prologue_count_inport[891+:3];
	assign prologue_count_wire[10][7] = prologue_count_inport[888+:3];
	assign prologue_count_wire[11][0] = prologue_count_inport[885+:3];
	assign prologue_count_wire[11][1] = prologue_count_inport[882+:3];
	assign prologue_count_wire[11][2] = prologue_count_inport[879+:3];
	assign prologue_count_wire[11][3] = prologue_count_inport[876+:3];
	assign prologue_count_wire[11][4] = prologue_count_inport[873+:3];
	assign prologue_count_wire[11][5] = prologue_count_inport[870+:3];
	assign prologue_count_wire[11][6] = prologue_count_inport[867+:3];
	assign prologue_count_wire[11][7] = prologue_count_inport[864+:3];
	assign prologue_count_wire[12][0] = prologue_count_inport[861+:3];
	assign prologue_count_wire[12][1] = prologue_count_inport[858+:3];
	assign prologue_count_wire[12][2] = prologue_count_inport[855+:3];
	assign prologue_count_wire[12][3] = prologue_count_inport[852+:3];
	assign prologue_count_wire[12][4] = prologue_count_inport[849+:3];
	assign prologue_count_wire[12][5] = prologue_count_inport[846+:3];
	assign prologue_count_wire[12][6] = prologue_count_inport[843+:3];
	assign prologue_count_wire[12][7] = prologue_count_inport[840+:3];
	assign prologue_count_wire[13][0] = prologue_count_inport[837+:3];
	assign prologue_count_wire[13][1] = prologue_count_inport[834+:3];
	assign prologue_count_wire[13][2] = prologue_count_inport[831+:3];
	assign prologue_count_wire[13][3] = prologue_count_inport[828+:3];
	assign prologue_count_wire[13][4] = prologue_count_inport[825+:3];
	assign prologue_count_wire[13][5] = prologue_count_inport[822+:3];
	assign prologue_count_wire[13][6] = prologue_count_inport[819+:3];
	assign prologue_count_wire[13][7] = prologue_count_inport[816+:3];
	assign prologue_count_wire[14][0] = prologue_count_inport[813+:3];
	assign prologue_count_wire[14][1] = prologue_count_inport[810+:3];
	assign prologue_count_wire[14][2] = prologue_count_inport[807+:3];
	assign prologue_count_wire[14][3] = prologue_count_inport[804+:3];
	assign prologue_count_wire[14][4] = prologue_count_inport[801+:3];
	assign prologue_count_wire[14][5] = prologue_count_inport[798+:3];
	assign prologue_count_wire[14][6] = prologue_count_inport[795+:3];
	assign prologue_count_wire[14][7] = prologue_count_inport[792+:3];
	assign prologue_count_wire[15][0] = prologue_count_inport[789+:3];
	assign prologue_count_wire[15][1] = prologue_count_inport[786+:3];
	assign prologue_count_wire[15][2] = prologue_count_inport[783+:3];
	assign prologue_count_wire[15][3] = prologue_count_inport[780+:3];
	assign prologue_count_wire[15][4] = prologue_count_inport[777+:3];
	assign prologue_count_wire[15][5] = prologue_count_inport[774+:3];
	assign prologue_count_wire[15][6] = prologue_count_inport[771+:3];
	assign prologue_count_wire[15][7] = prologue_count_inport[768+:3];
	assign prologue_count_wire[16][0] = prologue_count_inport[765+:3];
	assign prologue_count_wire[16][1] = prologue_count_inport[762+:3];
	assign prologue_count_wire[16][2] = prologue_count_inport[759+:3];
	assign prologue_count_wire[16][3] = prologue_count_inport[756+:3];
	assign prologue_count_wire[16][4] = prologue_count_inport[753+:3];
	assign prologue_count_wire[16][5] = prologue_count_inport[750+:3];
	assign prologue_count_wire[16][6] = prologue_count_inport[747+:3];
	assign prologue_count_wire[16][7] = prologue_count_inport[744+:3];
	assign prologue_count_wire[17][0] = prologue_count_inport[741+:3];
	assign prologue_count_wire[17][1] = prologue_count_inport[738+:3];
	assign prologue_count_wire[17][2] = prologue_count_inport[735+:3];
	assign prologue_count_wire[17][3] = prologue_count_inport[732+:3];
	assign prologue_count_wire[17][4] = prologue_count_inport[729+:3];
	assign prologue_count_wire[17][5] = prologue_count_inport[726+:3];
	assign prologue_count_wire[17][6] = prologue_count_inport[723+:3];
	assign prologue_count_wire[17][7] = prologue_count_inport[720+:3];
	assign prologue_count_wire[18][0] = prologue_count_inport[717+:3];
	assign prologue_count_wire[18][1] = prologue_count_inport[714+:3];
	assign prologue_count_wire[18][2] = prologue_count_inport[711+:3];
	assign prologue_count_wire[18][3] = prologue_count_inport[708+:3];
	assign prologue_count_wire[18][4] = prologue_count_inport[705+:3];
	assign prologue_count_wire[18][5] = prologue_count_inport[702+:3];
	assign prologue_count_wire[18][6] = prologue_count_inport[699+:3];
	assign prologue_count_wire[18][7] = prologue_count_inport[696+:3];
	assign prologue_count_wire[19][0] = prologue_count_inport[693+:3];
	assign prologue_count_wire[19][1] = prologue_count_inport[690+:3];
	assign prologue_count_wire[19][2] = prologue_count_inport[687+:3];
	assign prologue_count_wire[19][3] = prologue_count_inport[684+:3];
	assign prologue_count_wire[19][4] = prologue_count_inport[681+:3];
	assign prologue_count_wire[19][5] = prologue_count_inport[678+:3];
	assign prologue_count_wire[19][6] = prologue_count_inport[675+:3];
	assign prologue_count_wire[19][7] = prologue_count_inport[672+:3];
	assign prologue_count_wire[20][0] = prologue_count_inport[669+:3];
	assign prologue_count_wire[20][1] = prologue_count_inport[666+:3];
	assign prologue_count_wire[20][2] = prologue_count_inport[663+:3];
	assign prologue_count_wire[20][3] = prologue_count_inport[660+:3];
	assign prologue_count_wire[20][4] = prologue_count_inport[657+:3];
	assign prologue_count_wire[20][5] = prologue_count_inport[654+:3];
	assign prologue_count_wire[20][6] = prologue_count_inport[651+:3];
	assign prologue_count_wire[20][7] = prologue_count_inport[648+:3];
	assign prologue_count_wire[21][0] = prologue_count_inport[645+:3];
	assign prologue_count_wire[21][1] = prologue_count_inport[642+:3];
	assign prologue_count_wire[21][2] = prologue_count_inport[639+:3];
	assign prologue_count_wire[21][3] = prologue_count_inport[636+:3];
	assign prologue_count_wire[21][4] = prologue_count_inport[633+:3];
	assign prologue_count_wire[21][5] = prologue_count_inport[630+:3];
	assign prologue_count_wire[21][6] = prologue_count_inport[627+:3];
	assign prologue_count_wire[21][7] = prologue_count_inport[624+:3];
	assign prologue_count_wire[22][0] = prologue_count_inport[621+:3];
	assign prologue_count_wire[22][1] = prologue_count_inport[618+:3];
	assign prologue_count_wire[22][2] = prologue_count_inport[615+:3];
	assign prologue_count_wire[22][3] = prologue_count_inport[612+:3];
	assign prologue_count_wire[22][4] = prologue_count_inport[609+:3];
	assign prologue_count_wire[22][5] = prologue_count_inport[606+:3];
	assign prologue_count_wire[22][6] = prologue_count_inport[603+:3];
	assign prologue_count_wire[22][7] = prologue_count_inport[600+:3];
	assign prologue_count_wire[23][0] = prologue_count_inport[597+:3];
	assign prologue_count_wire[23][1] = prologue_count_inport[594+:3];
	assign prologue_count_wire[23][2] = prologue_count_inport[591+:3];
	assign prologue_count_wire[23][3] = prologue_count_inport[588+:3];
	assign prologue_count_wire[23][4] = prologue_count_inport[585+:3];
	assign prologue_count_wire[23][5] = prologue_count_inport[582+:3];
	assign prologue_count_wire[23][6] = prologue_count_inport[579+:3];
	assign prologue_count_wire[23][7] = prologue_count_inport[576+:3];
	assign prologue_count_wire[24][0] = prologue_count_inport[573+:3];
	assign prologue_count_wire[24][1] = prologue_count_inport[570+:3];
	assign prologue_count_wire[24][2] = prologue_count_inport[567+:3];
	assign prologue_count_wire[24][3] = prologue_count_inport[564+:3];
	assign prologue_count_wire[24][4] = prologue_count_inport[561+:3];
	assign prologue_count_wire[24][5] = prologue_count_inport[558+:3];
	assign prologue_count_wire[24][6] = prologue_count_inport[555+:3];
	assign prologue_count_wire[24][7] = prologue_count_inport[552+:3];
	assign prologue_count_wire[25][0] = prologue_count_inport[549+:3];
	assign prologue_count_wire[25][1] = prologue_count_inport[546+:3];
	assign prologue_count_wire[25][2] = prologue_count_inport[543+:3];
	assign prologue_count_wire[25][3] = prologue_count_inport[540+:3];
	assign prologue_count_wire[25][4] = prologue_count_inport[537+:3];
	assign prologue_count_wire[25][5] = prologue_count_inport[534+:3];
	assign prologue_count_wire[25][6] = prologue_count_inport[531+:3];
	assign prologue_count_wire[25][7] = prologue_count_inport[528+:3];
	assign prologue_count_wire[26][0] = prologue_count_inport[525+:3];
	assign prologue_count_wire[26][1] = prologue_count_inport[522+:3];
	assign prologue_count_wire[26][2] = prologue_count_inport[519+:3];
	assign prologue_count_wire[26][3] = prologue_count_inport[516+:3];
	assign prologue_count_wire[26][4] = prologue_count_inport[513+:3];
	assign prologue_count_wire[26][5] = prologue_count_inport[510+:3];
	assign prologue_count_wire[26][6] = prologue_count_inport[507+:3];
	assign prologue_count_wire[26][7] = prologue_count_inport[504+:3];
	assign prologue_count_wire[27][0] = prologue_count_inport[501+:3];
	assign prologue_count_wire[27][1] = prologue_count_inport[498+:3];
	assign prologue_count_wire[27][2] = prologue_count_inport[495+:3];
	assign prologue_count_wire[27][3] = prologue_count_inport[492+:3];
	assign prologue_count_wire[27][4] = prologue_count_inport[489+:3];
	assign prologue_count_wire[27][5] = prologue_count_inport[486+:3];
	assign prologue_count_wire[27][6] = prologue_count_inport[483+:3];
	assign prologue_count_wire[27][7] = prologue_count_inport[480+:3];
	assign prologue_count_wire[28][0] = prologue_count_inport[477+:3];
	assign prologue_count_wire[28][1] = prologue_count_inport[474+:3];
	assign prologue_count_wire[28][2] = prologue_count_inport[471+:3];
	assign prologue_count_wire[28][3] = prologue_count_inport[468+:3];
	assign prologue_count_wire[28][4] = prologue_count_inport[465+:3];
	assign prologue_count_wire[28][5] = prologue_count_inport[462+:3];
	assign prologue_count_wire[28][6] = prologue_count_inport[459+:3];
	assign prologue_count_wire[28][7] = prologue_count_inport[456+:3];
	assign prologue_count_wire[29][0] = prologue_count_inport[453+:3];
	assign prologue_count_wire[29][1] = prologue_count_inport[450+:3];
	assign prologue_count_wire[29][2] = prologue_count_inport[447+:3];
	assign prologue_count_wire[29][3] = prologue_count_inport[444+:3];
	assign prologue_count_wire[29][4] = prologue_count_inport[441+:3];
	assign prologue_count_wire[29][5] = prologue_count_inport[438+:3];
	assign prologue_count_wire[29][6] = prologue_count_inport[435+:3];
	assign prologue_count_wire[29][7] = prologue_count_inport[432+:3];
	assign prologue_count_wire[30][0] = prologue_count_inport[429+:3];
	assign prologue_count_wire[30][1] = prologue_count_inport[426+:3];
	assign prologue_count_wire[30][2] = prologue_count_inport[423+:3];
	assign prologue_count_wire[30][3] = prologue_count_inport[420+:3];
	assign prologue_count_wire[30][4] = prologue_count_inport[417+:3];
	assign prologue_count_wire[30][5] = prologue_count_inport[414+:3];
	assign prologue_count_wire[30][6] = prologue_count_inport[411+:3];
	assign prologue_count_wire[30][7] = prologue_count_inport[408+:3];
	assign prologue_count_wire[31][0] = prologue_count_inport[405+:3];
	assign prologue_count_wire[31][1] = prologue_count_inport[402+:3];
	assign prologue_count_wire[31][2] = prologue_count_inport[399+:3];
	assign prologue_count_wire[31][3] = prologue_count_inport[396+:3];
	assign prologue_count_wire[31][4] = prologue_count_inport[393+:3];
	assign prologue_count_wire[31][5] = prologue_count_inport[390+:3];
	assign prologue_count_wire[31][6] = prologue_count_inport[387+:3];
	assign prologue_count_wire[31][7] = prologue_count_inport[384+:3];
	assign prologue_count_wire[32][0] = prologue_count_inport[381+:3];
	assign prologue_count_wire[32][1] = prologue_count_inport[378+:3];
	assign prologue_count_wire[32][2] = prologue_count_inport[375+:3];
	assign prologue_count_wire[32][3] = prologue_count_inport[372+:3];
	assign prologue_count_wire[32][4] = prologue_count_inport[369+:3];
	assign prologue_count_wire[32][5] = prologue_count_inport[366+:3];
	assign prologue_count_wire[32][6] = prologue_count_inport[363+:3];
	assign prologue_count_wire[32][7] = prologue_count_inport[360+:3];
	assign prologue_count_wire[33][0] = prologue_count_inport[357+:3];
	assign prologue_count_wire[33][1] = prologue_count_inport[354+:3];
	assign prologue_count_wire[33][2] = prologue_count_inport[351+:3];
	assign prologue_count_wire[33][3] = prologue_count_inport[348+:3];
	assign prologue_count_wire[33][4] = prologue_count_inport[345+:3];
	assign prologue_count_wire[33][5] = prologue_count_inport[342+:3];
	assign prologue_count_wire[33][6] = prologue_count_inport[339+:3];
	assign prologue_count_wire[33][7] = prologue_count_inport[336+:3];
	assign prologue_count_wire[34][0] = prologue_count_inport[333+:3];
	assign prologue_count_wire[34][1] = prologue_count_inport[330+:3];
	assign prologue_count_wire[34][2] = prologue_count_inport[327+:3];
	assign prologue_count_wire[34][3] = prologue_count_inport[324+:3];
	assign prologue_count_wire[34][4] = prologue_count_inport[321+:3];
	assign prologue_count_wire[34][5] = prologue_count_inport[318+:3];
	assign prologue_count_wire[34][6] = prologue_count_inport[315+:3];
	assign prologue_count_wire[34][7] = prologue_count_inport[312+:3];
	assign prologue_count_wire[35][0] = prologue_count_inport[309+:3];
	assign prologue_count_wire[35][1] = prologue_count_inport[306+:3];
	assign prologue_count_wire[35][2] = prologue_count_inport[303+:3];
	assign prologue_count_wire[35][3] = prologue_count_inport[300+:3];
	assign prologue_count_wire[35][4] = prologue_count_inport[297+:3];
	assign prologue_count_wire[35][5] = prologue_count_inport[294+:3];
	assign prologue_count_wire[35][6] = prologue_count_inport[291+:3];
	assign prologue_count_wire[35][7] = prologue_count_inport[288+:3];
	assign prologue_count_wire[36][0] = prologue_count_inport[285+:3];
	assign prologue_count_wire[36][1] = prologue_count_inport[282+:3];
	assign prologue_count_wire[36][2] = prologue_count_inport[279+:3];
	assign prologue_count_wire[36][3] = prologue_count_inport[276+:3];
	assign prologue_count_wire[36][4] = prologue_count_inport[273+:3];
	assign prologue_count_wire[36][5] = prologue_count_inport[270+:3];
	assign prologue_count_wire[36][6] = prologue_count_inport[267+:3];
	assign prologue_count_wire[36][7] = prologue_count_inport[264+:3];
	assign prologue_count_wire[37][0] = prologue_count_inport[261+:3];
	assign prologue_count_wire[37][1] = prologue_count_inport[258+:3];
	assign prologue_count_wire[37][2] = prologue_count_inport[255+:3];
	assign prologue_count_wire[37][3] = prologue_count_inport[252+:3];
	assign prologue_count_wire[37][4] = prologue_count_inport[249+:3];
	assign prologue_count_wire[37][5] = prologue_count_inport[246+:3];
	assign prologue_count_wire[37][6] = prologue_count_inport[243+:3];
	assign prologue_count_wire[37][7] = prologue_count_inport[240+:3];
	assign prologue_count_wire[38][0] = prologue_count_inport[237+:3];
	assign prologue_count_wire[38][1] = prologue_count_inport[234+:3];
	assign prologue_count_wire[38][2] = prologue_count_inport[231+:3];
	assign prologue_count_wire[38][3] = prologue_count_inport[228+:3];
	assign prologue_count_wire[38][4] = prologue_count_inport[225+:3];
	assign prologue_count_wire[38][5] = prologue_count_inport[222+:3];
	assign prologue_count_wire[38][6] = prologue_count_inport[219+:3];
	assign prologue_count_wire[38][7] = prologue_count_inport[216+:3];
	assign prologue_count_wire[39][0] = prologue_count_inport[213+:3];
	assign prologue_count_wire[39][1] = prologue_count_inport[210+:3];
	assign prologue_count_wire[39][2] = prologue_count_inport[207+:3];
	assign prologue_count_wire[39][3] = prologue_count_inport[204+:3];
	assign prologue_count_wire[39][4] = prologue_count_inport[201+:3];
	assign prologue_count_wire[39][5] = prologue_count_inport[198+:3];
	assign prologue_count_wire[39][6] = prologue_count_inport[195+:3];
	assign prologue_count_wire[39][7] = prologue_count_inport[192+:3];
	assign prologue_count_wire[40][0] = prologue_count_inport[189+:3];
	assign prologue_count_wire[40][1] = prologue_count_inport[186+:3];
	assign prologue_count_wire[40][2] = prologue_count_inport[183+:3];
	assign prologue_count_wire[40][3] = prologue_count_inport[180+:3];
	assign prologue_count_wire[40][4] = prologue_count_inport[177+:3];
	assign prologue_count_wire[40][5] = prologue_count_inport[174+:3];
	assign prologue_count_wire[40][6] = prologue_count_inport[171+:3];
	assign prologue_count_wire[40][7] = prologue_count_inport[168+:3];
	assign prologue_count_wire[41][0] = prologue_count_inport[165+:3];
	assign prologue_count_wire[41][1] = prologue_count_inport[162+:3];
	assign prologue_count_wire[41][2] = prologue_count_inport[159+:3];
	assign prologue_count_wire[41][3] = prologue_count_inport[156+:3];
	assign prologue_count_wire[41][4] = prologue_count_inport[153+:3];
	assign prologue_count_wire[41][5] = prologue_count_inport[150+:3];
	assign prologue_count_wire[41][6] = prologue_count_inport[147+:3];
	assign prologue_count_wire[41][7] = prologue_count_inport[144+:3];
	assign prologue_count_wire[42][0] = prologue_count_inport[141+:3];
	assign prologue_count_wire[42][1] = prologue_count_inport[138+:3];
	assign prologue_count_wire[42][2] = prologue_count_inport[135+:3];
	assign prologue_count_wire[42][3] = prologue_count_inport[132+:3];
	assign prologue_count_wire[42][4] = prologue_count_inport[129+:3];
	assign prologue_count_wire[42][5] = prologue_count_inport[126+:3];
	assign prologue_count_wire[42][6] = prologue_count_inport[123+:3];
	assign prologue_count_wire[42][7] = prologue_count_inport[120+:3];
	assign prologue_count_wire[43][0] = prologue_count_inport[117+:3];
	assign prologue_count_wire[43][1] = prologue_count_inport[114+:3];
	assign prologue_count_wire[43][2] = prologue_count_inport[111+:3];
	assign prologue_count_wire[43][3] = prologue_count_inport[108+:3];
	assign prologue_count_wire[43][4] = prologue_count_inport[105+:3];
	assign prologue_count_wire[43][5] = prologue_count_inport[102+:3];
	assign prologue_count_wire[43][6] = prologue_count_inport[99+:3];
	assign prologue_count_wire[43][7] = prologue_count_inport[96+:3];
	assign prologue_count_wire[44][0] = prologue_count_inport[93+:3];
	assign prologue_count_wire[44][1] = prologue_count_inport[90+:3];
	assign prologue_count_wire[44][2] = prologue_count_inport[87+:3];
	assign prologue_count_wire[44][3] = prologue_count_inport[84+:3];
	assign prologue_count_wire[44][4] = prologue_count_inport[81+:3];
	assign prologue_count_wire[44][5] = prologue_count_inport[78+:3];
	assign prologue_count_wire[44][6] = prologue_count_inport[75+:3];
	assign prologue_count_wire[44][7] = prologue_count_inport[72+:3];
	assign prologue_count_wire[45][0] = prologue_count_inport[69+:3];
	assign prologue_count_wire[45][1] = prologue_count_inport[66+:3];
	assign prologue_count_wire[45][2] = prologue_count_inport[63+:3];
	assign prologue_count_wire[45][3] = prologue_count_inport[60+:3];
	assign prologue_count_wire[45][4] = prologue_count_inport[57+:3];
	assign prologue_count_wire[45][5] = prologue_count_inport[54+:3];
	assign prologue_count_wire[45][6] = prologue_count_inport[51+:3];
	assign prologue_count_wire[45][7] = prologue_count_inport[48+:3];
	assign prologue_count_wire[46][0] = prologue_count_inport[45+:3];
	assign prologue_count_wire[46][1] = prologue_count_inport[42+:3];
	assign prologue_count_wire[46][2] = prologue_count_inport[39+:3];
	assign prologue_count_wire[46][3] = prologue_count_inport[36+:3];
	assign prologue_count_wire[46][4] = prologue_count_inport[33+:3];
	assign prologue_count_wire[46][5] = prologue_count_inport[30+:3];
	assign prologue_count_wire[46][6] = prologue_count_inport[27+:3];
	assign prologue_count_wire[46][7] = prologue_count_inport[24+:3];
	assign prologue_count_wire[47][0] = prologue_count_inport[21+:3];
	assign prologue_count_wire[47][1] = prologue_count_inport[18+:3];
	assign prologue_count_wire[47][2] = prologue_count_inport[15+:3];
	assign prologue_count_wire[47][3] = prologue_count_inport[12+:3];
	assign prologue_count_wire[47][4] = prologue_count_inport[9+:3];
	assign prologue_count_wire[47][5] = prologue_count_inport[6+:3];
	assign prologue_count_wire[47][6] = prologue_count_inport[3+:3];
	assign prologue_count_wire[47][7] = prologue_count_inport[0+:3];
	initial _sv2v_0 = 0;
endmodule
module RegisterFile__da749a1852bb59a5 (
	clk,
	raddr,
	rdata,
	reset,
	waddr,
	wdata,
	wen
);
	reg _sv2v_0;
	input wire [0:0] clk;
	input wire [0:0] raddr;
	output reg [34:0] rdata;
	input wire [0:0] reset;
	input wire [0:0] waddr;
	input wire [34:0] wdata;
	input wire [0:0] wen;
	localparam [0:0] __const__rd_ports_at_up_rf_read = 1'd1;
	localparam [0:0] __const__wr_ports_at_up_rf_write = 1'd1;
	reg [34:0] regs [0:1];
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : up_rf_read
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__rd_ports_at_up_rf_read; i = i + 1'd1)
				rdata[sv2v_cast_1(i) * 35+:35] = regs[raddr[sv2v_cast_1(i)+:1]];
		end
	end
	always @(posedge clk) begin : up_rf_write
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__wr_ports_at_up_rf_write; i = i + 1'd1)
				if (wen[sv2v_cast_1(i)+:1])
					regs[waddr[sv2v_cast_1(i)+:1]] <= wdata[sv2v_cast_1(i) * 35+:35];
		end
	end
	initial _sv2v_0 = 0;
endmodule
module NormalQueueDpathRTL__e10c2d77bcb9538e (
	clk,
	raddr,
	recv_msg,
	reset,
	send_msg,
	waddr,
	wen
);
	input wire [0:0] clk;
	input wire [0:0] raddr;
	input wire [34:0] recv_msg;
	input wire [0:0] reset;
	output wire [34:0] send_msg;
	input wire [0:0] waddr;
	input wire [0:0] wen;
	wire [0:0] rf__clk;
	wire [0:0] rf__raddr;
	wire [34:0] rf__rdata;
	wire [0:0] rf__reset;
	wire [0:0] rf__waddr;
	wire [34:0] rf__wdata;
	wire [0:0] rf__wen;
	RegisterFile__da749a1852bb59a5 rf(
		.clk(rf__clk),
		.raddr(rf__raddr),
		.rdata(rf__rdata),
		.reset(rf__reset),
		.waddr(rf__waddr),
		.wdata(rf__wdata),
		.wen(rf__wen)
	);
	assign rf__clk = clk;
	assign rf__reset = reset;
	assign rf__raddr[0+:1] = raddr;
	assign send_msg = rf__rdata[0+:35];
	assign rf__wen[0+:1] = wen;
	assign rf__waddr[0+:1] = waddr;
	assign rf__wdata[0+:35] = recv_msg;
endmodule
module NormalQueueWithClearRTL__e10c2d77bcb9538e (
	clear,
	clk,
	count,
	reset,
	recv__msg,
	recv__rdy,
	recv__val,
	send__msg,
	send__rdy,
	send__val
);
	input wire [0:0] clear;
	input wire [0:0] clk;
	output wire [1:0] count;
	input wire [0:0] reset;
	input wire [34:0] recv__msg;
	output wire [0:0] recv__rdy;
	input wire [0:0] recv__val;
	output wire [34:0] send__msg;
	input wire [0:0] send__rdy;
	output wire [0:0] send__val;
	wire [0:0] ctrl__clear;
	wire [0:0] ctrl__clk;
	wire [1:0] ctrl__count;
	wire [0:0] ctrl__raddr;
	wire [0:0] ctrl__recv_rdy;
	wire [0:0] ctrl__recv_val;
	wire [0:0] ctrl__reset;
	wire [0:0] ctrl__send_rdy;
	wire [0:0] ctrl__send_val;
	wire [0:0] ctrl__waddr;
	wire [0:0] ctrl__wen;
	NormalQueueCtrlWithClearRTL__num_entries_2 ctrl(
		.clear(ctrl__clear),
		.clk(ctrl__clk),
		.count(ctrl__count),
		.raddr(ctrl__raddr),
		.recv_rdy(ctrl__recv_rdy),
		.recv_val(ctrl__recv_val),
		.reset(ctrl__reset),
		.send_rdy(ctrl__send_rdy),
		.send_val(ctrl__send_val),
		.waddr(ctrl__waddr),
		.wen(ctrl__wen)
	);
	wire [0:0] dpath__clk;
	wire [0:0] dpath__raddr;
	wire [34:0] dpath__recv_msg;
	wire [0:0] dpath__reset;
	wire [34:0] dpath__send_msg;
	wire [0:0] dpath__waddr;
	wire [0:0] dpath__wen;
	NormalQueueDpathRTL__e10c2d77bcb9538e dpath(
		.clk(dpath__clk),
		.raddr(dpath__raddr),
		.recv_msg(dpath__recv_msg),
		.reset(dpath__reset),
		.send_msg(dpath__send_msg),
		.waddr(dpath__waddr),
		.wen(dpath__wen)
	);
	assign ctrl__clk = clk;
	assign ctrl__reset = reset;
	assign dpath__clk = clk;
	assign dpath__reset = reset;
	assign dpath__wen = ctrl__wen;
	assign dpath__waddr = ctrl__waddr;
	assign dpath__raddr = ctrl__raddr;
	assign ctrl__recv_val = recv__val;
	assign recv__rdy = ctrl__recv_rdy;
	assign dpath__recv_msg = recv__msg;
	assign send__val = ctrl__send_val;
	assign ctrl__send_rdy = send__rdy;
	assign send__msg = dpath__send_msg;
	assign count = ctrl__count;
	assign ctrl__clear = clear;
endmodule
module ChannelWithClearRTL__28234a25712022e4 (
	clear,
	clk,
	reset,
	recv__msg,
	recv__rdy,
	recv__val,
	send__msg,
	send__rdy,
	send__val
);
	input wire [0:0] clear;
	input wire [0:0] clk;
	input wire [0:0] reset;
	input wire [34:0] recv__msg;
	output wire [0:0] recv__rdy;
	input wire [0:0] recv__val;
	output wire [34:0] send__msg;
	input wire [0:0] send__rdy;
	output wire [0:0] send__val;
	wire [0:0] queues__clear [0:0];
	wire [0:0] queues__clk [0:0];
	wire [1:0] queues__count [0:0];
	wire [0:0] queues__reset [0:0];
	wire [34:0] queues__recv__msg [0:0];
	wire [0:0] queues__recv__rdy [0:0];
	wire [0:0] queues__recv__val [0:0];
	wire [34:0] queues__send__msg [0:0];
	wire [0:0] queues__send__rdy [0:0];
	wire [0:0] queues__send__val [0:0];
	NormalQueueWithClearRTL__e10c2d77bcb9538e queues__0(
		.clear(queues__clear[0]),
		.clk(queues__clk[0]),
		.count(queues__count[0]),
		.reset(queues__reset[0]),
		.recv__msg(queues__recv__msg[0]),
		.recv__rdy(queues__recv__rdy[0]),
		.recv__val(queues__recv__val[0]),
		.send__msg(queues__send__msg[0]),
		.send__rdy(queues__send__rdy[0]),
		.send__val(queues__send__val[0])
	);
	assign queues__clk[0] = clk;
	assign queues__reset[0] = reset;
	assign queues__recv__msg[0] = recv__msg;
	assign recv__rdy = queues__recv__rdy[0];
	assign queues__recv__val[0] = recv__val;
	assign send__msg = queues__send__msg[0];
	assign queues__send__rdy[0] = send__rdy;
	assign send__val = queues__send__val[0];
	assign queues__clear[0] = clear;
endmodule
module LinkOrRTL__a54ca58e7852ace8 (
	clk,
	reset,
	recv_fu__msg,
	recv_fu__rdy,
	recv_fu__val,
	recv_xbar__msg,
	recv_xbar__rdy,
	recv_xbar__val,
	send__msg,
	send__rdy,
	send__val
);
	reg _sv2v_0;
	input wire [0:0] clk;
	input wire [0:0] reset;
	input wire [34:0] recv_fu__msg;
	output reg [0:0] recv_fu__rdy;
	input wire [0:0] recv_fu__val;
	input wire [34:0] recv_xbar__msg;
	output reg [0:0] recv_xbar__rdy;
	input wire [0:0] recv_xbar__val;
	output reg [34:0] send__msg;
	input wire [0:0] send__rdy;
	output reg [0:0] send__val;
	always @(*) begin : process
		if (_sv2v_0)
			;
		send__msg = 35'h000000000;
		send__msg[2] = recv_fu__msg[2] | recv_xbar__msg[2];
		send__msg[34-:32] = recv_xbar__msg[34-:32] | recv_fu__msg[34-:32];
		send__val = recv_fu__val | recv_xbar__val;
		recv_fu__rdy = send__rdy;
		recv_xbar__rdy = send__rdy;
	end
	initial _sv2v_0 = 0;
endmodule
module TileWithContextSwitchRTL__70d22822c0293fa6 (
	cgra_id,
	clk,
	reset,
	tile_id,
	from_mem_rdata__msg,
	from_mem_rdata__rdy,
	from_mem_rdata__val,
	recv_data__msg,
	recv_data__rdy,
	recv_data__val,
	recv_from_controller_pkt__msg,
	recv_from_controller_pkt__rdy,
	recv_from_controller_pkt__val,
	send_data__msg,
	send_data__rdy,
	send_data__val,
	send_to_controller_pkt__msg,
	send_to_controller_pkt__rdy,
	send_to_controller_pkt__val,
	to_mem_raddr__msg,
	to_mem_raddr__rdy,
	to_mem_raddr__val,
	to_mem_waddr__msg,
	to_mem_waddr__rdy,
	to_mem_waddr__val,
	to_mem_wdata__msg,
	to_mem_wdata__rdy,
	to_mem_wdata__val
);
	reg _sv2v_0;
	input wire [0:0] cgra_id;
	input wire [0:0] clk;
	input wire [0:0] reset;
	input wire [4:0] tile_id;
	input wire [34:0] from_mem_rdata__msg;
	output wire [0:0] from_mem_rdata__rdy;
	input wire [0:0] from_mem_rdata__val;
	input wire [139:0] recv_data__msg;
	output reg [3:0] recv_data__rdy;
	input wire [3:0] recv_data__val;
	input wire [197:0] recv_from_controller_pkt__msg;
	output reg [0:0] recv_from_controller_pkt__rdy;
	input wire [0:0] recv_from_controller_pkt__val;
	output wire [139:0] send_data__msg;
	input wire [3:0] send_data__rdy;
	output wire [3:0] send_data__val;
	output reg [197:0] send_to_controller_pkt__msg;
	input wire [0:0] send_to_controller_pkt__rdy;
	output reg [0:0] send_to_controller_pkt__val;
	output wire [14:0] to_mem_raddr__msg;
	input wire [0:0] to_mem_raddr__rdy;
	output wire [0:0] to_mem_raddr__val;
	output wire [14:0] to_mem_waddr__msg;
	input wire [0:0] to_mem_waddr__rdy;
	output wire [0:0] to_mem_waddr__val;
	output wire [34:0] to_mem_wdata__msg;
	input wire [0:0] to_mem_wdata__rdy;
	output wire [0:0] to_mem_wdata__val;
	localparam [2:0] __const__num_tile_inports_at_isolate_inputs = 3'd4;
	localparam [5:0] __const__CMD_REARM = 6'd59;
	localparam [0:0] __const__CMD_LAUNCH = 1'd0;
	localparam [1:0] __const__CMD_CONFIG = 2'd3;
	localparam [2:0] __const__CMD_CONFIG_PROLOGUE_FU = 3'd4;
	localparam [2:0] __const__CMD_CONFIG_PROLOGUE_FU_CROSSBAR = 3'd5;
	localparam [2:0] __const__CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR = 3'd6;
	localparam [2:0] __const__CMD_CONFIG_TOTAL_CTRL_COUNT = 3'd7;
	localparam [3:0] __const__CMD_CONFIG_COUNT_PER_ITER = 4'd8;
	localparam [3:0] __const__CMD_CONFIG_CTRL_LOWER_BOUND = 4'd9;
	localparam [4:0] __const__CMD_GLOBAL_REDUCE_ADD_RESPONSE = 5'd20;
	localparam [4:0] __const__CMD_GLOBAL_REDUCE_MUL_RESPONSE = 5'd21;
	localparam [4:0] __const__CMD_CONFIG_STREAMING_LD_START_ADDR = 5'd23;
	localparam [4:0] __const__CMD_CONFIG_STREAMING_LD_STRIDE = 5'd24;
	localparam [4:0] __const__CMD_CONFIG_STREAMING_LD_END_ADDR = 5'd25;
	localparam [4:0] __const__CMD_CONFIG_LOOP_LOWER = 5'd28;
	localparam [4:0] __const__CMD_CONFIG_LOOP_UPPER = 5'd29;
	localparam [4:0] __const__CMD_CONFIG_LOOP_STEP = 5'd30;
	localparam [5:0] __const__CMD_CONFIG_GEP_STRIDE = 6'd43;
	localparam [0:0] __const__has_context_switch_at_feed_pkt = 1'd0;
	localparam [4:0] __const__CMD_RECORD_PHI_ADDR = 5'd16;
	localparam [0:0] __const__CMD_PAUSE = 1'd1;
	localparam [4:0] __const__CMD_PRESERVE = 5'd22;
	localparam [3:0] __const__CMD_RESUME = 4'd15;
	localparam [3:0] __const__CMD_CONST = 4'd13;
	localparam [1:0] __const__CMD_TERMINATE = 2'd2;
	reg [0:0] clear;
	reg [0:0] element_done;
	reg [0:0] fu_crossbar_done;
	reg [0:0] hold_data;
	reg [0:0] rearm_pending;
	reg [0:0] routing_crossbar_done;
	wire [0:0] const_mem__clear;
	wire [0:0] const_mem__clk;
	reg [0:0] const_mem__ctrl_proceed;
	wire [0:0] const_mem__reset;
	reg [34:0] const_mem__recv_const__msg;
	wire [0:0] const_mem__recv_const__rdy;
	reg [0:0] const_mem__recv_const__val;
	wire [34:0] const_mem__send_const__msg;
	wire [0:0] const_mem__send_const__rdy;
	wire [0:0] const_mem__send_const__val;
	ConstQueueDynamicRTL__9e94e78d19eca926 const_mem(
		.clear(const_mem__clear),
		.clk(const_mem__clk),
		.ctrl_proceed(const_mem__ctrl_proceed),
		.reset(const_mem__reset),
		.recv_const__msg(const_mem__recv_const__msg),
		.recv_const__rdy(const_mem__recv_const__rdy),
		.recv_const__val(const_mem__recv_const__val),
		.send_const__msg(const_mem__send_const__msg),
		.send_const__rdy(const_mem__send_const__rdy),
		.send_const__val(const_mem__send_const__val)
	);
	wire [0:0] ctrl_mem__cgra_id;
	wire [0:0] ctrl_mem__clk;
	wire [5:0] ctrl_mem__ctrl_addr_outport;
	wire [2:0] ctrl_mem__prologue_count_outport_fu;
	wire [287:0] ctrl_mem__prologue_count_outport_fu_crossbar;
	wire [1151:0] ctrl_mem__prologue_count_outport_routing_crossbar;
	wire [0:0] ctrl_mem__rearm;
	wire [0:0] ctrl_mem__reset;
	wire [4:0] ctrl_mem__tile_id;
	wire [172:0] ctrl_mem__recv_from_element__msg;
	wire [0:0] ctrl_mem__recv_from_element__rdy;
	wire [0:0] ctrl_mem__recv_from_element__val;
	reg [197:0] ctrl_mem__recv_pkt_from_controller__msg;
	wire [0:0] ctrl_mem__recv_pkt_from_controller__rdy;
	reg [0:0] ctrl_mem__recv_pkt_from_controller__val;
	wire [110:0] ctrl_mem__send_ctrl__msg;
	reg [0:0] ctrl_mem__send_ctrl__rdy;
	wire [0:0] ctrl_mem__send_ctrl__val;
	wire [197:0] ctrl_mem__send_pkt_to_controller__msg;
	reg [0:0] ctrl_mem__send_pkt_to_controller__rdy;
	wire [0:0] ctrl_mem__send_pkt_to_controller__val;
	wire [172:0] ctrl_mem__send_to_element__msg;
	wire [0:0] ctrl_mem__send_to_element__rdy;
	wire [0:0] ctrl_mem__send_to_element__val;
	CtrlMemDynamicRTL__df08502b9dfbead5 ctrl_mem(
		.cgra_id(ctrl_mem__cgra_id),
		.clk(ctrl_mem__clk),
		.ctrl_addr_outport(ctrl_mem__ctrl_addr_outport),
		.prologue_count_outport_fu(ctrl_mem__prologue_count_outport_fu),
		.prologue_count_outport_fu_crossbar(ctrl_mem__prologue_count_outport_fu_crossbar),
		.prologue_count_outport_routing_crossbar(ctrl_mem__prologue_count_outport_routing_crossbar),
		.rearm(ctrl_mem__rearm),
		.reset(ctrl_mem__reset),
		.tile_id(ctrl_mem__tile_id),
		.recv_from_element__msg(ctrl_mem__recv_from_element__msg),
		.recv_from_element__rdy(ctrl_mem__recv_from_element__rdy),
		.recv_from_element__val(ctrl_mem__recv_from_element__val),
		.recv_pkt_from_controller__msg(ctrl_mem__recv_pkt_from_controller__msg),
		.recv_pkt_from_controller__rdy(ctrl_mem__recv_pkt_from_controller__rdy),
		.recv_pkt_from_controller__val(ctrl_mem__recv_pkt_from_controller__val),
		.send_ctrl__msg(ctrl_mem__send_ctrl__msg),
		.send_ctrl__rdy(ctrl_mem__send_ctrl__rdy),
		.send_ctrl__val(ctrl_mem__send_ctrl__val),
		.send_pkt_to_controller__msg(ctrl_mem__send_pkt_to_controller__msg),
		.send_pkt_to_controller__rdy(ctrl_mem__send_pkt_to_controller__rdy),
		.send_pkt_to_controller__val(ctrl_mem__send_pkt_to_controller__val),
		.send_to_element__msg(ctrl_mem__send_to_element__msg),
		.send_to_element__rdy(ctrl_mem__send_to_element__rdy),
		.send_to_element__val(ctrl_mem__send_to_element__val)
	);
	wire [12:0] element__clear;
	wire [0:0] element__clk;
	wire [5:0] element__ctrl_addr_inport;
	wire [2:0] element__prologue_count_inport;
	wire [0:0] element__reset;
	wire [4:0] element__tile_id;
	wire [454:0] element__from_mem_rdata__msg;
	wire [12:0] element__from_mem_rdata__rdy;
	wire [12:0] element__from_mem_rdata__val;
	wire [34:0] element__recv_const__msg;
	wire [0:0] element__recv_const__rdy;
	wire [0:0] element__recv_const__val;
	wire [172:0] element__recv_from_ctrl_mem__msg;
	wire [0:0] element__recv_from_ctrl_mem__rdy;
	wire [0:0] element__recv_from_ctrl_mem__val;
	wire [139:0] element__recv_in__msg;
	wire [3:0] element__recv_in__rdy;
	wire [3:0] element__recv_in__val;
	reg [110:0] element__recv_opt__msg;
	wire [0:0] element__recv_opt__rdy;
	reg [0:0] element__recv_opt__val;
	wire [197:0] element__recv_pkt_from_controller__msg;
	wire [0:0] element__recv_pkt_from_controller__rdy;
	wire [0:0] element__recv_pkt_from_controller__val;
	wire [69:0] element__send_out__msg;
	wire [1:0] element__send_out__rdy;
	wire [1:0] element__send_out__val;
	wire [172:0] element__send_to_ctrl_mem__msg;
	wire [0:0] element__send_to_ctrl_mem__rdy;
	wire [0:0] element__send_to_ctrl_mem__val;
	wire [194:0] element__to_mem_raddr__msg;
	wire [12:0] element__to_mem_raddr__rdy;
	wire [12:0] element__to_mem_raddr__val;
	wire [194:0] element__to_mem_waddr__msg;
	wire [12:0] element__to_mem_waddr__rdy;
	wire [12:0] element__to_mem_waddr__val;
	wire [454:0] element__to_mem_wdata__msg;
	wire [12:0] element__to_mem_wdata__rdy;
	wire [12:0] element__to_mem_wdata__val;
	FlexibleFuRTL__8889a6e6a7910850 element(
		.clear(element__clear),
		.clk(element__clk),
		.ctrl_addr_inport(element__ctrl_addr_inport),
		.prologue_count_inport(element__prologue_count_inport),
		.reset(element__reset),
		.tile_id(element__tile_id),
		.from_mem_rdata__msg(element__from_mem_rdata__msg),
		.from_mem_rdata__rdy(element__from_mem_rdata__rdy),
		.from_mem_rdata__val(element__from_mem_rdata__val),
		.recv_const__msg(element__recv_const__msg),
		.recv_const__rdy(element__recv_const__rdy),
		.recv_const__val(element__recv_const__val),
		.recv_from_ctrl_mem__msg(element__recv_from_ctrl_mem__msg),
		.recv_from_ctrl_mem__rdy(element__recv_from_ctrl_mem__rdy),
		.recv_from_ctrl_mem__val(element__recv_from_ctrl_mem__val),
		.recv_in__msg(element__recv_in__msg),
		.recv_in__rdy(element__recv_in__rdy),
		.recv_in__val(element__recv_in__val),
		.recv_opt__msg(element__recv_opt__msg),
		.recv_opt__rdy(element__recv_opt__rdy),
		.recv_opt__val(element__recv_opt__val),
		.recv_pkt_from_controller__msg(element__recv_pkt_from_controller__msg),
		.recv_pkt_from_controller__rdy(element__recv_pkt_from_controller__rdy),
		.recv_pkt_from_controller__val(element__recv_pkt_from_controller__val),
		.send_out__msg(element__send_out__msg),
		.send_out__rdy(element__send_out__rdy),
		.send_out__val(element__send_out__val),
		.send_to_ctrl_mem__msg(element__send_to_ctrl_mem__msg),
		.send_to_ctrl_mem__rdy(element__send_to_ctrl_mem__rdy),
		.send_to_ctrl_mem__val(element__send_to_ctrl_mem__val),
		.to_mem_raddr__msg(element__to_mem_raddr__msg),
		.to_mem_raddr__rdy(element__to_mem_raddr__rdy),
		.to_mem_raddr__val(element__to_mem_raddr__val),
		.to_mem_waddr__msg(element__to_mem_waddr__msg),
		.to_mem_waddr__rdy(element__to_mem_waddr__rdy),
		.to_mem_waddr__val(element__to_mem_waddr__val),
		.to_mem_wdata__msg(element__to_mem_wdata__msg),
		.to_mem_wdata__rdy(element__to_mem_wdata__rdy),
		.to_mem_wdata__val(element__to_mem_wdata__val)
	);
	wire [0:0] fu_crossbar__cgra_id;
	wire [0:0] fu_crossbar__clear;
	wire [0:0] fu_crossbar__clk;
	reg [0:0] fu_crossbar__compute_done;
	wire [0:0] fu_crossbar__crossbar_id;
	wire [15:0] fu_crossbar__crossbar_outport;
	wire [5:0] fu_crossbar__ctrl_addr_inport;
	wire [287:0] fu_crossbar__prologue_count_inport;
	wire [0:0] fu_crossbar__reset;
	wire [4:0] fu_crossbar__tile_id;
	wire [69:0] fu_crossbar__recv_data__msg;
	wire [1:0] fu_crossbar__recv_data__rdy;
	wire [1:0] fu_crossbar__recv_data__val;
	reg [110:0] fu_crossbar__recv_opt__msg;
	wire [0:0] fu_crossbar__recv_opt__rdy;
	reg [0:0] fu_crossbar__recv_opt__val;
	wire [279:0] fu_crossbar__send_data__msg;
	wire [7:0] fu_crossbar__send_data__rdy;
	wire [7:0] fu_crossbar__send_data__val;
	CrossbarRTL__af5511e92d2ad43c fu_crossbar(
		.cgra_id(fu_crossbar__cgra_id),
		.clear(fu_crossbar__clear),
		.clk(fu_crossbar__clk),
		.compute_done(fu_crossbar__compute_done),
		.crossbar_id(fu_crossbar__crossbar_id),
		.crossbar_outport(fu_crossbar__crossbar_outport),
		.ctrl_addr_inport(fu_crossbar__ctrl_addr_inport),
		.prologue_count_inport(fu_crossbar__prologue_count_inport),
		.reset(fu_crossbar__reset),
		.tile_id(fu_crossbar__tile_id),
		.recv_data__msg(fu_crossbar__recv_data__msg),
		.recv_data__rdy(fu_crossbar__recv_data__rdy),
		.recv_data__val(fu_crossbar__recv_data__val),
		.recv_opt__msg(fu_crossbar__recv_opt__msg),
		.recv_opt__rdy(fu_crossbar__recv_opt__rdy),
		.recv_opt__val(fu_crossbar__recv_opt__val),
		.send_data__msg(fu_crossbar__send_data__msg),
		.send_data__rdy(fu_crossbar__send_data__rdy),
		.send_data__val(fu_crossbar__send_data__val)
	);
	wire [0:0] register_cluster__clear;
	wire [0:0] register_cluster__clk;
	reg [0:0] register_cluster__inport_ctrl_proceed;
	wire [110:0] register_cluster__inport_opt;
	wire [0:0] register_cluster__reset;
	wire [139:0] register_cluster__recv_data_from_const__msg;
	wire [3:0] register_cluster__recv_data_from_const__rdy;
	wire [3:0] register_cluster__recv_data_from_const__val;
	wire [139:0] register_cluster__recv_data_from_fu_crossbar__msg;
	wire [3:0] register_cluster__recv_data_from_fu_crossbar__rdy;
	wire [3:0] register_cluster__recv_data_from_fu_crossbar__val;
	wire [139:0] register_cluster__recv_data_from_routing_crossbar__msg;
	wire [3:0] register_cluster__recv_data_from_routing_crossbar__rdy;
	wire [3:0] register_cluster__recv_data_from_routing_crossbar__val;
	wire [139:0] register_cluster__send_data_to_fu__msg;
	wire [3:0] register_cluster__send_data_to_fu__rdy;
	wire [3:0] register_cluster__send_data_to_fu__val;
	wire [139:0] register_cluster__send_data_to_routing_crossbar__msg;
	wire [3:0] register_cluster__send_data_to_routing_crossbar__rdy;
	wire [3:0] register_cluster__send_data_to_routing_crossbar__val;
	RegisterClusterRTL__4cdc70707e060b34 register_cluster(
		.clear(register_cluster__clear),
		.clk(register_cluster__clk),
		.inport_ctrl_proceed(register_cluster__inport_ctrl_proceed),
		.inport_opt(register_cluster__inport_opt),
		.reset(register_cluster__reset),
		.recv_data_from_const__msg(register_cluster__recv_data_from_const__msg),
		.recv_data_from_const__rdy(register_cluster__recv_data_from_const__rdy),
		.recv_data_from_const__val(register_cluster__recv_data_from_const__val),
		.recv_data_from_fu_crossbar__msg(register_cluster__recv_data_from_fu_crossbar__msg),
		.recv_data_from_fu_crossbar__rdy(register_cluster__recv_data_from_fu_crossbar__rdy),
		.recv_data_from_fu_crossbar__val(register_cluster__recv_data_from_fu_crossbar__val),
		.recv_data_from_routing_crossbar__msg(register_cluster__recv_data_from_routing_crossbar__msg),
		.recv_data_from_routing_crossbar__rdy(register_cluster__recv_data_from_routing_crossbar__rdy),
		.recv_data_from_routing_crossbar__val(register_cluster__recv_data_from_routing_crossbar__val),
		.send_data_to_fu__msg(register_cluster__send_data_to_fu__msg),
		.send_data_to_fu__rdy(register_cluster__send_data_to_fu__rdy),
		.send_data_to_fu__val(register_cluster__send_data_to_fu__val),
		.send_data_to_routing_crossbar__msg(register_cluster__send_data_to_routing_crossbar__msg),
		.send_data_to_routing_crossbar__rdy(register_cluster__send_data_to_routing_crossbar__rdy),
		.send_data_to_routing_crossbar__val(register_cluster__send_data_to_routing_crossbar__val)
	);
	wire [0:0] routing_crossbar__cgra_id;
	wire [0:0] routing_crossbar__clear;
	wire [0:0] routing_crossbar__clk;
	reg [0:0] routing_crossbar__compute_done;
	wire [0:0] routing_crossbar__crossbar_id;
	wire [31:0] routing_crossbar__crossbar_outport;
	wire [5:0] routing_crossbar__ctrl_addr_inport;
	wire [1151:0] routing_crossbar__prologue_count_inport;
	wire [0:0] routing_crossbar__reset;
	wire [4:0] routing_crossbar__tile_id;
	wire [279:0] routing_crossbar__recv_data__msg;
	wire [7:0] routing_crossbar__recv_data__rdy;
	wire [7:0] routing_crossbar__recv_data__val;
	reg [110:0] routing_crossbar__recv_opt__msg;
	wire [0:0] routing_crossbar__recv_opt__rdy;
	reg [0:0] routing_crossbar__recv_opt__val;
	wire [279:0] routing_crossbar__send_data__msg;
	wire [7:0] routing_crossbar__send_data__rdy;
	wire [7:0] routing_crossbar__send_data__val;
	CrossbarRTL__f9cc749ddff26d17 routing_crossbar(
		.cgra_id(routing_crossbar__cgra_id),
		.clear(routing_crossbar__clear),
		.clk(routing_crossbar__clk),
		.compute_done(routing_crossbar__compute_done),
		.crossbar_id(routing_crossbar__crossbar_id),
		.crossbar_outport(routing_crossbar__crossbar_outport),
		.ctrl_addr_inport(routing_crossbar__ctrl_addr_inport),
		.prologue_count_inport(routing_crossbar__prologue_count_inport),
		.reset(routing_crossbar__reset),
		.tile_id(routing_crossbar__tile_id),
		.recv_data__msg(routing_crossbar__recv_data__msg),
		.recv_data__rdy(routing_crossbar__recv_data__rdy),
		.recv_data__val(routing_crossbar__recv_data__val),
		.recv_opt__msg(routing_crossbar__recv_opt__msg),
		.recv_opt__rdy(routing_crossbar__recv_opt__rdy),
		.recv_opt__val(routing_crossbar__recv_opt__val),
		.send_data__msg(routing_crossbar__send_data__msg),
		.send_data__rdy(routing_crossbar__send_data__rdy),
		.send_data__val(routing_crossbar__send_data__val)
	);
	wire [0:0] tile_in_channel__clear [0:3];
	wire [0:0] tile_in_channel__clk [0:3];
	wire [0:0] tile_in_channel__reset [0:3];
	wire [34:0] tile_in_channel__recv__msg [0:3];
	wire [0:0] tile_in_channel__recv__rdy [0:3];
	reg [0:0] tile_in_channel__recv__val [0:3];
	wire [34:0] tile_in_channel__send__msg [0:3];
	wire [0:0] tile_in_channel__send__rdy [0:3];
	wire [0:0] tile_in_channel__send__val [0:3];
	ChannelWithClearRTL__28234a25712022e4 tile_in_channel__0(
		.clear(tile_in_channel__clear[0]),
		.clk(tile_in_channel__clk[0]),
		.reset(tile_in_channel__reset[0]),
		.recv__msg(tile_in_channel__recv__msg[0]),
		.recv__rdy(tile_in_channel__recv__rdy[0]),
		.recv__val(tile_in_channel__recv__val[0]),
		.send__msg(tile_in_channel__send__msg[0]),
		.send__rdy(tile_in_channel__send__rdy[0]),
		.send__val(tile_in_channel__send__val[0])
	);
	ChannelWithClearRTL__28234a25712022e4 tile_in_channel__1(
		.clear(tile_in_channel__clear[1]),
		.clk(tile_in_channel__clk[1]),
		.reset(tile_in_channel__reset[1]),
		.recv__msg(tile_in_channel__recv__msg[1]),
		.recv__rdy(tile_in_channel__recv__rdy[1]),
		.recv__val(tile_in_channel__recv__val[1]),
		.send__msg(tile_in_channel__send__msg[1]),
		.send__rdy(tile_in_channel__send__rdy[1]),
		.send__val(tile_in_channel__send__val[1])
	);
	ChannelWithClearRTL__28234a25712022e4 tile_in_channel__2(
		.clear(tile_in_channel__clear[2]),
		.clk(tile_in_channel__clk[2]),
		.reset(tile_in_channel__reset[2]),
		.recv__msg(tile_in_channel__recv__msg[2]),
		.recv__rdy(tile_in_channel__recv__rdy[2]),
		.recv__val(tile_in_channel__recv__val[2]),
		.send__msg(tile_in_channel__send__msg[2]),
		.send__rdy(tile_in_channel__send__rdy[2]),
		.send__val(tile_in_channel__send__val[2])
	);
	ChannelWithClearRTL__28234a25712022e4 tile_in_channel__3(
		.clear(tile_in_channel__clear[3]),
		.clk(tile_in_channel__clk[3]),
		.reset(tile_in_channel__reset[3]),
		.recv__msg(tile_in_channel__recv__msg[3]),
		.recv__rdy(tile_in_channel__recv__rdy[3]),
		.recv__val(tile_in_channel__recv__val[3]),
		.send__msg(tile_in_channel__send__msg[3]),
		.send__rdy(tile_in_channel__send__rdy[3]),
		.send__val(tile_in_channel__send__val[3])
	);
	wire [0:0] tile_out_or_link__clk [0:3];
	wire [0:0] tile_out_or_link__reset [0:3];
	wire [34:0] tile_out_or_link__recv_fu__msg [0:3];
	wire [0:0] tile_out_or_link__recv_fu__rdy [0:3];
	wire [0:0] tile_out_or_link__recv_fu__val [0:3];
	wire [34:0] tile_out_or_link__recv_xbar__msg [0:3];
	wire [0:0] tile_out_or_link__recv_xbar__rdy [0:3];
	wire [0:0] tile_out_or_link__recv_xbar__val [0:3];
	wire [34:0] tile_out_or_link__send__msg [0:3];
	wire [0:0] tile_out_or_link__send__rdy [0:3];
	wire [0:0] tile_out_or_link__send__val [0:3];
	LinkOrRTL__a54ca58e7852ace8 tile_out_or_link__0(
		.clk(tile_out_or_link__clk[0]),
		.reset(tile_out_or_link__reset[0]),
		.recv_fu__msg(tile_out_or_link__recv_fu__msg[0]),
		.recv_fu__rdy(tile_out_or_link__recv_fu__rdy[0]),
		.recv_fu__val(tile_out_or_link__recv_fu__val[0]),
		.recv_xbar__msg(tile_out_or_link__recv_xbar__msg[0]),
		.recv_xbar__rdy(tile_out_or_link__recv_xbar__rdy[0]),
		.recv_xbar__val(tile_out_or_link__recv_xbar__val[0]),
		.send__msg(tile_out_or_link__send__msg[0]),
		.send__rdy(tile_out_or_link__send__rdy[0]),
		.send__val(tile_out_or_link__send__val[0])
	);
	LinkOrRTL__a54ca58e7852ace8 tile_out_or_link__1(
		.clk(tile_out_or_link__clk[1]),
		.reset(tile_out_or_link__reset[1]),
		.recv_fu__msg(tile_out_or_link__recv_fu__msg[1]),
		.recv_fu__rdy(tile_out_or_link__recv_fu__rdy[1]),
		.recv_fu__val(tile_out_or_link__recv_fu__val[1]),
		.recv_xbar__msg(tile_out_or_link__recv_xbar__msg[1]),
		.recv_xbar__rdy(tile_out_or_link__recv_xbar__rdy[1]),
		.recv_xbar__val(tile_out_or_link__recv_xbar__val[1]),
		.send__msg(tile_out_or_link__send__msg[1]),
		.send__rdy(tile_out_or_link__send__rdy[1]),
		.send__val(tile_out_or_link__send__val[1])
	);
	LinkOrRTL__a54ca58e7852ace8 tile_out_or_link__2(
		.clk(tile_out_or_link__clk[2]),
		.reset(tile_out_or_link__reset[2]),
		.recv_fu__msg(tile_out_or_link__recv_fu__msg[2]),
		.recv_fu__rdy(tile_out_or_link__recv_fu__rdy[2]),
		.recv_fu__val(tile_out_or_link__recv_fu__val[2]),
		.recv_xbar__msg(tile_out_or_link__recv_xbar__msg[2]),
		.recv_xbar__rdy(tile_out_or_link__recv_xbar__rdy[2]),
		.recv_xbar__val(tile_out_or_link__recv_xbar__val[2]),
		.send__msg(tile_out_or_link__send__msg[2]),
		.send__rdy(tile_out_or_link__send__rdy[2]),
		.send__val(tile_out_or_link__send__val[2])
	);
	LinkOrRTL__a54ca58e7852ace8 tile_out_or_link__3(
		.clk(tile_out_or_link__clk[3]),
		.reset(tile_out_or_link__reset[3]),
		.recv_fu__msg(tile_out_or_link__recv_fu__msg[3]),
		.recv_fu__rdy(tile_out_or_link__recv_fu__rdy[3]),
		.recv_fu__val(tile_out_or_link__recv_fu__val[3]),
		.recv_xbar__msg(tile_out_or_link__recv_xbar__msg[3]),
		.recv_xbar__rdy(tile_out_or_link__recv_xbar__rdy[3]),
		.recv_xbar__val(tile_out_or_link__recv_xbar__val[3]),
		.send__msg(tile_out_or_link__send__msg[3]),
		.send__rdy(tile_out_or_link__send__rdy[3]),
		.send__val(tile_out_or_link__send__val[3])
	);
	function automatic [5:0] sv2v_cast_6;
		input reg [5:0] inp;
		sv2v_cast_6 = inp;
	endfunction
	always @(*) begin : feed_pkt
		if (_sv2v_0)
			;
		ctrl_mem__recv_pkt_from_controller__msg = 198'h00000000000000000000000000000000000000000000000000;
		const_mem__recv_const__msg = 35'h000000000;
		ctrl_mem__recv_pkt_from_controller__val = 1'd0;
		const_mem__recv_const__val = 1'd0;
		recv_from_controller_pkt__rdy = 1'd0;
		if (recv_from_controller_pkt__val & (((((((((((((((((((recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG)) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_PROLOGUE_FU))) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_PROLOGUE_FU_CROSSBAR))) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR))) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_TOTAL_CTRL_COUNT))) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_COUNT_PER_ITER))) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_CTRL_LOWER_BOUND))) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_GLOBAL_REDUCE_ADD_RESPONSE))) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_GLOBAL_REDUCE_MUL_RESPONSE))) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_STREAMING_LD_START_ADDR))) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_STREAMING_LD_STRIDE))) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_STREAMING_LD_END_ADDR))) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_LAUNCH))) | (recv_from_controller_pkt__msg[172-:6] == __const__CMD_REARM)) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_LOOP_LOWER))) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_LOOP_UPPER))) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_CONFIG_LOOP_STEP))) | (recv_from_controller_pkt__msg[172-:6] == __const__CMD_CONFIG_GEP_STRIDE)) | (__const__has_context_switch_at_feed_pkt & ((((recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_RECORD_PHI_ADDR)) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_PAUSE))) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_PRESERVE))) | (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_RESUME)))))) begin
			ctrl_mem__recv_pkt_from_controller__val = 1'd1;
			ctrl_mem__recv_pkt_from_controller__msg = recv_from_controller_pkt__msg;
			recv_from_controller_pkt__rdy = ctrl_mem__recv_pkt_from_controller__rdy;
		end
		else if (recv_from_controller_pkt__val & (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_CONST))) begin
			const_mem__recv_const__val = 1'd1;
			const_mem__recv_const__msg = recv_from_controller_pkt__msg[166-:35];
			recv_from_controller_pkt__rdy = const_mem__recv_const__rdy;
		end
		if (recv_from_controller_pkt__val & (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_TERMINATE))) begin
			ctrl_mem__recv_pkt_from_controller__val = 1'd1;
			ctrl_mem__recv_pkt_from_controller__msg = recv_from_controller_pkt__msg;
			recv_from_controller_pkt__rdy = ctrl_mem__recv_pkt_from_controller__rdy;
			clear = 1'd1;
		end
		else
			clear = ctrl_mem__rearm;
		if (rearm_pending) begin
			ctrl_mem__recv_pkt_from_controller__val = 1'd0;
			const_mem__recv_const__val = 1'd0;
			recv_from_controller_pkt__rdy = 1'd0;
		end
	end
	function automatic [1:0] sv2v_cast_2;
		input reg [1:0] inp;
		sv2v_cast_2 = inp;
	endfunction
	always @(*) begin : isolate_inputs
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__num_tile_inports_at_isolate_inputs; i = i + 1'd1)
				begin
					tile_in_channel__recv__val[sv2v_cast_2(i)] = recv_data__val[3 - sv2v_cast_2(i)+:1] & ~(hold_data | ctrl_mem__rearm);
					recv_data__rdy[3 - sv2v_cast_2(i)+:1] = tile_in_channel__recv__rdy[sv2v_cast_2(i)] & ~(hold_data | ctrl_mem__rearm);
				end
		end
	end
	always @(*) begin : notify_const_mem
		if (_sv2v_0)
			;
		const_mem__ctrl_proceed = ctrl_mem__send_ctrl__rdy & ctrl_mem__send_ctrl__val;
		register_cluster__inport_ctrl_proceed = ctrl_mem__send_ctrl__rdy & ctrl_mem__send_ctrl__val;
	end
	always @(*) begin : notify_crossbars_compute_status
		if (_sv2v_0)
			;
		routing_crossbar__compute_done = element_done;
		fu_crossbar__compute_done = element_done;
	end
	always @(*) begin : update_opt
		if (_sv2v_0)
			;
		element__recv_opt__msg = ctrl_mem__send_ctrl__msg;
		routing_crossbar__recv_opt__msg = ctrl_mem__send_ctrl__msg;
		fu_crossbar__recv_opt__msg = ctrl_mem__send_ctrl__msg;
		element__recv_opt__val = ctrl_mem__send_ctrl__val & ~element_done;
		routing_crossbar__recv_opt__val = ctrl_mem__send_ctrl__val & ~routing_crossbar_done;
		fu_crossbar__recv_opt__val = ctrl_mem__send_ctrl__val & ~fu_crossbar_done;
		ctrl_mem__send_ctrl__rdy = ((element__recv_opt__rdy | element_done) & (routing_crossbar__recv_opt__rdy | routing_crossbar_done)) & (fu_crossbar__recv_opt__rdy | fu_crossbar_done);
	end
	always @(*) begin : update_send_out_signal
		if (_sv2v_0)
			;
		send_to_controller_pkt__val = 1'd0;
		send_to_controller_pkt__msg = 198'h00000000000000000000000000000000000000000000000000;
		if (ctrl_mem__send_pkt_to_controller__val) begin
			send_to_controller_pkt__val = 1'd1;
			send_to_controller_pkt__msg = ctrl_mem__send_pkt_to_controller__msg;
		end
		ctrl_mem__send_pkt_to_controller__rdy = send_to_controller_pkt__rdy;
	end
	always @(posedge clk) begin : already_done
		if ((reset | ctrl_mem__send_ctrl__rdy) | clear) begin
			element_done <= 1'd0;
			fu_crossbar_done <= 1'd0;
			routing_crossbar_done <= 1'd0;
		end
		else begin
			if (element__recv_opt__rdy)
				element_done <= 1'd1;
			if (fu_crossbar__recv_opt__rdy)
				fu_crossbar_done <= 1'd1;
			if (routing_crossbar__recv_opt__rdy)
				routing_crossbar_done <= 1'd1;
		end
	end
	always @(posedge clk) begin : hold_inputs
		if (reset)
			hold_data <= 1'd0;
		else if (ctrl_mem__rearm)
			hold_data <= 1'd1;
		else if ((recv_from_controller_pkt__val & recv_from_controller_pkt__rdy) & (recv_from_controller_pkt__msg[172-:6] == sv2v_cast_6(__const__CMD_LAUNCH)))
			hold_data <= 1'd0;
	end
	always @(posedge clk) begin : order_rearm
		if (reset | ctrl_mem__rearm)
			rearm_pending <= 1'd0;
		else if ((recv_from_controller_pkt__val & recv_from_controller_pkt__rdy) & (recv_from_controller_pkt__msg[172-:6] == __const__CMD_REARM))
			rearm_pending <= 1'd1;
	end
	assign element__clk = clk;
	assign element__reset = reset;
	assign const_mem__clk = clk;
	assign const_mem__reset = reset;
	assign routing_crossbar__clk = clk;
	assign routing_crossbar__reset = reset;
	assign fu_crossbar__clk = clk;
	assign fu_crossbar__reset = reset;
	assign register_cluster__clk = clk;
	assign register_cluster__reset = reset;
	assign ctrl_mem__clk = clk;
	assign ctrl_mem__reset = reset;
	assign tile_in_channel__clk[0] = clk;
	assign tile_in_channel__reset[0] = reset;
	assign tile_in_channel__clk[1] = clk;
	assign tile_in_channel__reset[1] = reset;
	assign tile_in_channel__clk[2] = clk;
	assign tile_in_channel__reset[2] = reset;
	assign tile_in_channel__clk[3] = clk;
	assign tile_in_channel__reset[3] = reset;
	assign tile_out_or_link__clk[0] = clk;
	assign tile_out_or_link__reset[0] = reset;
	assign tile_out_or_link__clk[1] = clk;
	assign tile_out_or_link__reset[1] = reset;
	assign tile_out_or_link__clk[2] = clk;
	assign tile_out_or_link__reset[2] = reset;
	assign tile_out_or_link__clk[3] = clk;
	assign tile_out_or_link__reset[3] = reset;
	assign element__tile_id = tile_id;
	assign ctrl_mem__cgra_id = cgra_id;
	assign ctrl_mem__tile_id = tile_id;
	assign fu_crossbar__cgra_id = cgra_id;
	assign fu_crossbar__tile_id = tile_id;
	assign routing_crossbar__cgra_id = cgra_id;
	assign routing_crossbar__tile_id = tile_id;
	assign routing_crossbar__crossbar_id = 1'd0;
	assign fu_crossbar__crossbar_id = 1'd1;
	assign element__recv_const__msg = const_mem__send_const__msg;
	assign const_mem__send_const__rdy = element__recv_const__rdy;
	assign element__recv_const__val = const_mem__send_const__val;
	assign ctrl_mem__recv_from_element__msg = element__send_to_ctrl_mem__msg;
	assign element__send_to_ctrl_mem__rdy = ctrl_mem__recv_from_element__rdy;
	assign ctrl_mem__recv_from_element__val = element__send_to_ctrl_mem__val;
	assign element__recv_from_ctrl_mem__msg = ctrl_mem__send_to_element__msg;
	assign ctrl_mem__send_to_element__rdy = element__recv_from_ctrl_mem__rdy;
	assign element__recv_from_ctrl_mem__val = ctrl_mem__send_to_element__val;
	assign routing_crossbar__ctrl_addr_inport = ctrl_mem__ctrl_addr_outport;
	assign fu_crossbar__ctrl_addr_inport = ctrl_mem__ctrl_addr_outport;
	assign element__ctrl_addr_inport = ctrl_mem__ctrl_addr_outport;
	assign element__prologue_count_inport = ctrl_mem__prologue_count_outport_fu;
	assign routing_crossbar__prologue_count_inport[1149+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1149+:3];
	assign routing_crossbar__prologue_count_inport[1146+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1146+:3];
	assign routing_crossbar__prologue_count_inport[1143+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1143+:3];
	assign routing_crossbar__prologue_count_inport[1140+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1140+:3];
	assign routing_crossbar__prologue_count_inport[1137+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1137+:3];
	assign routing_crossbar__prologue_count_inport[1134+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1134+:3];
	assign routing_crossbar__prologue_count_inport[1131+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1131+:3];
	assign routing_crossbar__prologue_count_inport[1128+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1128+:3];
	assign fu_crossbar__prologue_count_inport[285+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[285+:3];
	assign fu_crossbar__prologue_count_inport[282+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[282+:3];
	assign routing_crossbar__prologue_count_inport[1125+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1125+:3];
	assign routing_crossbar__prologue_count_inport[1122+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1122+:3];
	assign routing_crossbar__prologue_count_inport[1119+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1119+:3];
	assign routing_crossbar__prologue_count_inport[1116+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1116+:3];
	assign routing_crossbar__prologue_count_inport[1113+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1113+:3];
	assign routing_crossbar__prologue_count_inport[1110+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1110+:3];
	assign routing_crossbar__prologue_count_inport[1107+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1107+:3];
	assign routing_crossbar__prologue_count_inport[1104+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1104+:3];
	assign fu_crossbar__prologue_count_inport[279+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[279+:3];
	assign fu_crossbar__prologue_count_inport[276+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[276+:3];
	assign routing_crossbar__prologue_count_inport[1101+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1101+:3];
	assign routing_crossbar__prologue_count_inport[1098+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1098+:3];
	assign routing_crossbar__prologue_count_inport[1095+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1095+:3];
	assign routing_crossbar__prologue_count_inport[1092+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1092+:3];
	assign routing_crossbar__prologue_count_inport[1089+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1089+:3];
	assign routing_crossbar__prologue_count_inport[1086+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1086+:3];
	assign routing_crossbar__prologue_count_inport[1083+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1083+:3];
	assign routing_crossbar__prologue_count_inport[1080+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1080+:3];
	assign fu_crossbar__prologue_count_inport[273+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[273+:3];
	assign fu_crossbar__prologue_count_inport[270+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[270+:3];
	assign routing_crossbar__prologue_count_inport[1077+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1077+:3];
	assign routing_crossbar__prologue_count_inport[1074+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1074+:3];
	assign routing_crossbar__prologue_count_inport[1071+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1071+:3];
	assign routing_crossbar__prologue_count_inport[1068+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1068+:3];
	assign routing_crossbar__prologue_count_inport[1065+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1065+:3];
	assign routing_crossbar__prologue_count_inport[1062+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1062+:3];
	assign routing_crossbar__prologue_count_inport[1059+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1059+:3];
	assign routing_crossbar__prologue_count_inport[1056+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1056+:3];
	assign fu_crossbar__prologue_count_inport[267+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[267+:3];
	assign fu_crossbar__prologue_count_inport[264+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[264+:3];
	assign routing_crossbar__prologue_count_inport[1053+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1053+:3];
	assign routing_crossbar__prologue_count_inport[1050+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1050+:3];
	assign routing_crossbar__prologue_count_inport[1047+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1047+:3];
	assign routing_crossbar__prologue_count_inport[1044+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1044+:3];
	assign routing_crossbar__prologue_count_inport[1041+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1041+:3];
	assign routing_crossbar__prologue_count_inport[1038+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1038+:3];
	assign routing_crossbar__prologue_count_inport[1035+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1035+:3];
	assign routing_crossbar__prologue_count_inport[1032+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1032+:3];
	assign fu_crossbar__prologue_count_inport[261+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[261+:3];
	assign fu_crossbar__prologue_count_inport[258+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[258+:3];
	assign routing_crossbar__prologue_count_inport[1029+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1029+:3];
	assign routing_crossbar__prologue_count_inport[1026+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1026+:3];
	assign routing_crossbar__prologue_count_inport[1023+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1023+:3];
	assign routing_crossbar__prologue_count_inport[1020+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1020+:3];
	assign routing_crossbar__prologue_count_inport[1017+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1017+:3];
	assign routing_crossbar__prologue_count_inport[1014+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1014+:3];
	assign routing_crossbar__prologue_count_inport[1011+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1011+:3];
	assign routing_crossbar__prologue_count_inport[1008+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1008+:3];
	assign fu_crossbar__prologue_count_inport[255+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[255+:3];
	assign fu_crossbar__prologue_count_inport[252+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[252+:3];
	assign routing_crossbar__prologue_count_inport[1005+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1005+:3];
	assign routing_crossbar__prologue_count_inport[1002+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[1002+:3];
	assign routing_crossbar__prologue_count_inport[999+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[999+:3];
	assign routing_crossbar__prologue_count_inport[996+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[996+:3];
	assign routing_crossbar__prologue_count_inport[993+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[993+:3];
	assign routing_crossbar__prologue_count_inport[990+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[990+:3];
	assign routing_crossbar__prologue_count_inport[987+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[987+:3];
	assign routing_crossbar__prologue_count_inport[984+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[984+:3];
	assign fu_crossbar__prologue_count_inport[249+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[249+:3];
	assign fu_crossbar__prologue_count_inport[246+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[246+:3];
	assign routing_crossbar__prologue_count_inport[981+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[981+:3];
	assign routing_crossbar__prologue_count_inport[978+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[978+:3];
	assign routing_crossbar__prologue_count_inport[975+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[975+:3];
	assign routing_crossbar__prologue_count_inport[972+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[972+:3];
	assign routing_crossbar__prologue_count_inport[969+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[969+:3];
	assign routing_crossbar__prologue_count_inport[966+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[966+:3];
	assign routing_crossbar__prologue_count_inport[963+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[963+:3];
	assign routing_crossbar__prologue_count_inport[960+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[960+:3];
	assign fu_crossbar__prologue_count_inport[243+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[243+:3];
	assign fu_crossbar__prologue_count_inport[240+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[240+:3];
	assign routing_crossbar__prologue_count_inport[957+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[957+:3];
	assign routing_crossbar__prologue_count_inport[954+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[954+:3];
	assign routing_crossbar__prologue_count_inport[951+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[951+:3];
	assign routing_crossbar__prologue_count_inport[948+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[948+:3];
	assign routing_crossbar__prologue_count_inport[945+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[945+:3];
	assign routing_crossbar__prologue_count_inport[942+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[942+:3];
	assign routing_crossbar__prologue_count_inport[939+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[939+:3];
	assign routing_crossbar__prologue_count_inport[936+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[936+:3];
	assign fu_crossbar__prologue_count_inport[237+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[237+:3];
	assign fu_crossbar__prologue_count_inport[234+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[234+:3];
	assign routing_crossbar__prologue_count_inport[933+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[933+:3];
	assign routing_crossbar__prologue_count_inport[930+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[930+:3];
	assign routing_crossbar__prologue_count_inport[927+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[927+:3];
	assign routing_crossbar__prologue_count_inport[924+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[924+:3];
	assign routing_crossbar__prologue_count_inport[921+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[921+:3];
	assign routing_crossbar__prologue_count_inport[918+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[918+:3];
	assign routing_crossbar__prologue_count_inport[915+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[915+:3];
	assign routing_crossbar__prologue_count_inport[912+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[912+:3];
	assign fu_crossbar__prologue_count_inport[231+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[231+:3];
	assign fu_crossbar__prologue_count_inport[228+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[228+:3];
	assign routing_crossbar__prologue_count_inport[909+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[909+:3];
	assign routing_crossbar__prologue_count_inport[906+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[906+:3];
	assign routing_crossbar__prologue_count_inport[903+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[903+:3];
	assign routing_crossbar__prologue_count_inport[900+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[900+:3];
	assign routing_crossbar__prologue_count_inport[897+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[897+:3];
	assign routing_crossbar__prologue_count_inport[894+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[894+:3];
	assign routing_crossbar__prologue_count_inport[891+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[891+:3];
	assign routing_crossbar__prologue_count_inport[888+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[888+:3];
	assign fu_crossbar__prologue_count_inport[225+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[225+:3];
	assign fu_crossbar__prologue_count_inport[222+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[222+:3];
	assign routing_crossbar__prologue_count_inport[885+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[885+:3];
	assign routing_crossbar__prologue_count_inport[882+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[882+:3];
	assign routing_crossbar__prologue_count_inport[879+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[879+:3];
	assign routing_crossbar__prologue_count_inport[876+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[876+:3];
	assign routing_crossbar__prologue_count_inport[873+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[873+:3];
	assign routing_crossbar__prologue_count_inport[870+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[870+:3];
	assign routing_crossbar__prologue_count_inport[867+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[867+:3];
	assign routing_crossbar__prologue_count_inport[864+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[864+:3];
	assign fu_crossbar__prologue_count_inport[219+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[219+:3];
	assign fu_crossbar__prologue_count_inport[216+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[216+:3];
	assign routing_crossbar__prologue_count_inport[861+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[861+:3];
	assign routing_crossbar__prologue_count_inport[858+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[858+:3];
	assign routing_crossbar__prologue_count_inport[855+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[855+:3];
	assign routing_crossbar__prologue_count_inport[852+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[852+:3];
	assign routing_crossbar__prologue_count_inport[849+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[849+:3];
	assign routing_crossbar__prologue_count_inport[846+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[846+:3];
	assign routing_crossbar__prologue_count_inport[843+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[843+:3];
	assign routing_crossbar__prologue_count_inport[840+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[840+:3];
	assign fu_crossbar__prologue_count_inport[213+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[213+:3];
	assign fu_crossbar__prologue_count_inport[210+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[210+:3];
	assign routing_crossbar__prologue_count_inport[837+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[837+:3];
	assign routing_crossbar__prologue_count_inport[834+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[834+:3];
	assign routing_crossbar__prologue_count_inport[831+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[831+:3];
	assign routing_crossbar__prologue_count_inport[828+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[828+:3];
	assign routing_crossbar__prologue_count_inport[825+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[825+:3];
	assign routing_crossbar__prologue_count_inport[822+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[822+:3];
	assign routing_crossbar__prologue_count_inport[819+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[819+:3];
	assign routing_crossbar__prologue_count_inport[816+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[816+:3];
	assign fu_crossbar__prologue_count_inport[207+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[207+:3];
	assign fu_crossbar__prologue_count_inport[204+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[204+:3];
	assign routing_crossbar__prologue_count_inport[813+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[813+:3];
	assign routing_crossbar__prologue_count_inport[810+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[810+:3];
	assign routing_crossbar__prologue_count_inport[807+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[807+:3];
	assign routing_crossbar__prologue_count_inport[804+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[804+:3];
	assign routing_crossbar__prologue_count_inport[801+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[801+:3];
	assign routing_crossbar__prologue_count_inport[798+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[798+:3];
	assign routing_crossbar__prologue_count_inport[795+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[795+:3];
	assign routing_crossbar__prologue_count_inport[792+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[792+:3];
	assign fu_crossbar__prologue_count_inport[201+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[201+:3];
	assign fu_crossbar__prologue_count_inport[198+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[198+:3];
	assign routing_crossbar__prologue_count_inport[789+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[789+:3];
	assign routing_crossbar__prologue_count_inport[786+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[786+:3];
	assign routing_crossbar__prologue_count_inport[783+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[783+:3];
	assign routing_crossbar__prologue_count_inport[780+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[780+:3];
	assign routing_crossbar__prologue_count_inport[777+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[777+:3];
	assign routing_crossbar__prologue_count_inport[774+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[774+:3];
	assign routing_crossbar__prologue_count_inport[771+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[771+:3];
	assign routing_crossbar__prologue_count_inport[768+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[768+:3];
	assign fu_crossbar__prologue_count_inport[195+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[195+:3];
	assign fu_crossbar__prologue_count_inport[192+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[192+:3];
	assign routing_crossbar__prologue_count_inport[765+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[765+:3];
	assign routing_crossbar__prologue_count_inport[762+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[762+:3];
	assign routing_crossbar__prologue_count_inport[759+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[759+:3];
	assign routing_crossbar__prologue_count_inport[756+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[756+:3];
	assign routing_crossbar__prologue_count_inport[753+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[753+:3];
	assign routing_crossbar__prologue_count_inport[750+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[750+:3];
	assign routing_crossbar__prologue_count_inport[747+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[747+:3];
	assign routing_crossbar__prologue_count_inport[744+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[744+:3];
	assign fu_crossbar__prologue_count_inport[189+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[189+:3];
	assign fu_crossbar__prologue_count_inport[186+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[186+:3];
	assign routing_crossbar__prologue_count_inport[741+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[741+:3];
	assign routing_crossbar__prologue_count_inport[738+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[738+:3];
	assign routing_crossbar__prologue_count_inport[735+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[735+:3];
	assign routing_crossbar__prologue_count_inport[732+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[732+:3];
	assign routing_crossbar__prologue_count_inport[729+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[729+:3];
	assign routing_crossbar__prologue_count_inport[726+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[726+:3];
	assign routing_crossbar__prologue_count_inport[723+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[723+:3];
	assign routing_crossbar__prologue_count_inport[720+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[720+:3];
	assign fu_crossbar__prologue_count_inport[183+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[183+:3];
	assign fu_crossbar__prologue_count_inport[180+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[180+:3];
	assign routing_crossbar__prologue_count_inport[717+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[717+:3];
	assign routing_crossbar__prologue_count_inport[714+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[714+:3];
	assign routing_crossbar__prologue_count_inport[711+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[711+:3];
	assign routing_crossbar__prologue_count_inport[708+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[708+:3];
	assign routing_crossbar__prologue_count_inport[705+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[705+:3];
	assign routing_crossbar__prologue_count_inport[702+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[702+:3];
	assign routing_crossbar__prologue_count_inport[699+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[699+:3];
	assign routing_crossbar__prologue_count_inport[696+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[696+:3];
	assign fu_crossbar__prologue_count_inport[177+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[177+:3];
	assign fu_crossbar__prologue_count_inport[174+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[174+:3];
	assign routing_crossbar__prologue_count_inport[693+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[693+:3];
	assign routing_crossbar__prologue_count_inport[690+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[690+:3];
	assign routing_crossbar__prologue_count_inport[687+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[687+:3];
	assign routing_crossbar__prologue_count_inport[684+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[684+:3];
	assign routing_crossbar__prologue_count_inport[681+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[681+:3];
	assign routing_crossbar__prologue_count_inport[678+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[678+:3];
	assign routing_crossbar__prologue_count_inport[675+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[675+:3];
	assign routing_crossbar__prologue_count_inport[672+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[672+:3];
	assign fu_crossbar__prologue_count_inport[171+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[171+:3];
	assign fu_crossbar__prologue_count_inport[168+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[168+:3];
	assign routing_crossbar__prologue_count_inport[669+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[669+:3];
	assign routing_crossbar__prologue_count_inport[666+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[666+:3];
	assign routing_crossbar__prologue_count_inport[663+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[663+:3];
	assign routing_crossbar__prologue_count_inport[660+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[660+:3];
	assign routing_crossbar__prologue_count_inport[657+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[657+:3];
	assign routing_crossbar__prologue_count_inport[654+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[654+:3];
	assign routing_crossbar__prologue_count_inport[651+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[651+:3];
	assign routing_crossbar__prologue_count_inport[648+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[648+:3];
	assign fu_crossbar__prologue_count_inport[165+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[165+:3];
	assign fu_crossbar__prologue_count_inport[162+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[162+:3];
	assign routing_crossbar__prologue_count_inport[645+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[645+:3];
	assign routing_crossbar__prologue_count_inport[642+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[642+:3];
	assign routing_crossbar__prologue_count_inport[639+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[639+:3];
	assign routing_crossbar__prologue_count_inport[636+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[636+:3];
	assign routing_crossbar__prologue_count_inport[633+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[633+:3];
	assign routing_crossbar__prologue_count_inport[630+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[630+:3];
	assign routing_crossbar__prologue_count_inport[627+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[627+:3];
	assign routing_crossbar__prologue_count_inport[624+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[624+:3];
	assign fu_crossbar__prologue_count_inport[159+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[159+:3];
	assign fu_crossbar__prologue_count_inport[156+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[156+:3];
	assign routing_crossbar__prologue_count_inport[621+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[621+:3];
	assign routing_crossbar__prologue_count_inport[618+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[618+:3];
	assign routing_crossbar__prologue_count_inport[615+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[615+:3];
	assign routing_crossbar__prologue_count_inport[612+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[612+:3];
	assign routing_crossbar__prologue_count_inport[609+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[609+:3];
	assign routing_crossbar__prologue_count_inport[606+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[606+:3];
	assign routing_crossbar__prologue_count_inport[603+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[603+:3];
	assign routing_crossbar__prologue_count_inport[600+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[600+:3];
	assign fu_crossbar__prologue_count_inport[153+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[153+:3];
	assign fu_crossbar__prologue_count_inport[150+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[150+:3];
	assign routing_crossbar__prologue_count_inport[597+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[597+:3];
	assign routing_crossbar__prologue_count_inport[594+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[594+:3];
	assign routing_crossbar__prologue_count_inport[591+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[591+:3];
	assign routing_crossbar__prologue_count_inport[588+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[588+:3];
	assign routing_crossbar__prologue_count_inport[585+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[585+:3];
	assign routing_crossbar__prologue_count_inport[582+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[582+:3];
	assign routing_crossbar__prologue_count_inport[579+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[579+:3];
	assign routing_crossbar__prologue_count_inport[576+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[576+:3];
	assign fu_crossbar__prologue_count_inport[147+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[147+:3];
	assign fu_crossbar__prologue_count_inport[144+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[144+:3];
	assign routing_crossbar__prologue_count_inport[573+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[573+:3];
	assign routing_crossbar__prologue_count_inport[570+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[570+:3];
	assign routing_crossbar__prologue_count_inport[567+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[567+:3];
	assign routing_crossbar__prologue_count_inport[564+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[564+:3];
	assign routing_crossbar__prologue_count_inport[561+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[561+:3];
	assign routing_crossbar__prologue_count_inport[558+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[558+:3];
	assign routing_crossbar__prologue_count_inport[555+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[555+:3];
	assign routing_crossbar__prologue_count_inport[552+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[552+:3];
	assign fu_crossbar__prologue_count_inport[141+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[141+:3];
	assign fu_crossbar__prologue_count_inport[138+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[138+:3];
	assign routing_crossbar__prologue_count_inport[549+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[549+:3];
	assign routing_crossbar__prologue_count_inport[546+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[546+:3];
	assign routing_crossbar__prologue_count_inport[543+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[543+:3];
	assign routing_crossbar__prologue_count_inport[540+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[540+:3];
	assign routing_crossbar__prologue_count_inport[537+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[537+:3];
	assign routing_crossbar__prologue_count_inport[534+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[534+:3];
	assign routing_crossbar__prologue_count_inport[531+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[531+:3];
	assign routing_crossbar__prologue_count_inport[528+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[528+:3];
	assign fu_crossbar__prologue_count_inport[135+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[135+:3];
	assign fu_crossbar__prologue_count_inport[132+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[132+:3];
	assign routing_crossbar__prologue_count_inport[525+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[525+:3];
	assign routing_crossbar__prologue_count_inport[522+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[522+:3];
	assign routing_crossbar__prologue_count_inport[519+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[519+:3];
	assign routing_crossbar__prologue_count_inport[516+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[516+:3];
	assign routing_crossbar__prologue_count_inport[513+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[513+:3];
	assign routing_crossbar__prologue_count_inport[510+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[510+:3];
	assign routing_crossbar__prologue_count_inport[507+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[507+:3];
	assign routing_crossbar__prologue_count_inport[504+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[504+:3];
	assign fu_crossbar__prologue_count_inport[129+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[129+:3];
	assign fu_crossbar__prologue_count_inport[126+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[126+:3];
	assign routing_crossbar__prologue_count_inport[501+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[501+:3];
	assign routing_crossbar__prologue_count_inport[498+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[498+:3];
	assign routing_crossbar__prologue_count_inport[495+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[495+:3];
	assign routing_crossbar__prologue_count_inport[492+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[492+:3];
	assign routing_crossbar__prologue_count_inport[489+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[489+:3];
	assign routing_crossbar__prologue_count_inport[486+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[486+:3];
	assign routing_crossbar__prologue_count_inport[483+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[483+:3];
	assign routing_crossbar__prologue_count_inport[480+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[480+:3];
	assign fu_crossbar__prologue_count_inport[123+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[123+:3];
	assign fu_crossbar__prologue_count_inport[120+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[120+:3];
	assign routing_crossbar__prologue_count_inport[477+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[477+:3];
	assign routing_crossbar__prologue_count_inport[474+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[474+:3];
	assign routing_crossbar__prologue_count_inport[471+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[471+:3];
	assign routing_crossbar__prologue_count_inport[468+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[468+:3];
	assign routing_crossbar__prologue_count_inport[465+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[465+:3];
	assign routing_crossbar__prologue_count_inport[462+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[462+:3];
	assign routing_crossbar__prologue_count_inport[459+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[459+:3];
	assign routing_crossbar__prologue_count_inport[456+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[456+:3];
	assign fu_crossbar__prologue_count_inport[117+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[117+:3];
	assign fu_crossbar__prologue_count_inport[114+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[114+:3];
	assign routing_crossbar__prologue_count_inport[453+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[453+:3];
	assign routing_crossbar__prologue_count_inport[450+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[450+:3];
	assign routing_crossbar__prologue_count_inport[447+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[447+:3];
	assign routing_crossbar__prologue_count_inport[444+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[444+:3];
	assign routing_crossbar__prologue_count_inport[441+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[441+:3];
	assign routing_crossbar__prologue_count_inport[438+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[438+:3];
	assign routing_crossbar__prologue_count_inport[435+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[435+:3];
	assign routing_crossbar__prologue_count_inport[432+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[432+:3];
	assign fu_crossbar__prologue_count_inport[111+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[111+:3];
	assign fu_crossbar__prologue_count_inport[108+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[108+:3];
	assign routing_crossbar__prologue_count_inport[429+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[429+:3];
	assign routing_crossbar__prologue_count_inport[426+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[426+:3];
	assign routing_crossbar__prologue_count_inport[423+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[423+:3];
	assign routing_crossbar__prologue_count_inport[420+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[420+:3];
	assign routing_crossbar__prologue_count_inport[417+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[417+:3];
	assign routing_crossbar__prologue_count_inport[414+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[414+:3];
	assign routing_crossbar__prologue_count_inport[411+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[411+:3];
	assign routing_crossbar__prologue_count_inport[408+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[408+:3];
	assign fu_crossbar__prologue_count_inport[105+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[105+:3];
	assign fu_crossbar__prologue_count_inport[102+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[102+:3];
	assign routing_crossbar__prologue_count_inport[405+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[405+:3];
	assign routing_crossbar__prologue_count_inport[402+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[402+:3];
	assign routing_crossbar__prologue_count_inport[399+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[399+:3];
	assign routing_crossbar__prologue_count_inport[396+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[396+:3];
	assign routing_crossbar__prologue_count_inport[393+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[393+:3];
	assign routing_crossbar__prologue_count_inport[390+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[390+:3];
	assign routing_crossbar__prologue_count_inport[387+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[387+:3];
	assign routing_crossbar__prologue_count_inport[384+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[384+:3];
	assign fu_crossbar__prologue_count_inport[99+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[99+:3];
	assign fu_crossbar__prologue_count_inport[96+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[96+:3];
	assign routing_crossbar__prologue_count_inport[381+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[381+:3];
	assign routing_crossbar__prologue_count_inport[378+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[378+:3];
	assign routing_crossbar__prologue_count_inport[375+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[375+:3];
	assign routing_crossbar__prologue_count_inport[372+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[372+:3];
	assign routing_crossbar__prologue_count_inport[369+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[369+:3];
	assign routing_crossbar__prologue_count_inport[366+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[366+:3];
	assign routing_crossbar__prologue_count_inport[363+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[363+:3];
	assign routing_crossbar__prologue_count_inport[360+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[360+:3];
	assign fu_crossbar__prologue_count_inport[93+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[93+:3];
	assign fu_crossbar__prologue_count_inport[90+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[90+:3];
	assign routing_crossbar__prologue_count_inport[357+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[357+:3];
	assign routing_crossbar__prologue_count_inport[354+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[354+:3];
	assign routing_crossbar__prologue_count_inport[351+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[351+:3];
	assign routing_crossbar__prologue_count_inport[348+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[348+:3];
	assign routing_crossbar__prologue_count_inport[345+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[345+:3];
	assign routing_crossbar__prologue_count_inport[342+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[342+:3];
	assign routing_crossbar__prologue_count_inport[339+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[339+:3];
	assign routing_crossbar__prologue_count_inport[336+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[336+:3];
	assign fu_crossbar__prologue_count_inport[87+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[87+:3];
	assign fu_crossbar__prologue_count_inport[84+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[84+:3];
	assign routing_crossbar__prologue_count_inport[333+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[333+:3];
	assign routing_crossbar__prologue_count_inport[330+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[330+:3];
	assign routing_crossbar__prologue_count_inport[327+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[327+:3];
	assign routing_crossbar__prologue_count_inport[324+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[324+:3];
	assign routing_crossbar__prologue_count_inport[321+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[321+:3];
	assign routing_crossbar__prologue_count_inport[318+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[318+:3];
	assign routing_crossbar__prologue_count_inport[315+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[315+:3];
	assign routing_crossbar__prologue_count_inport[312+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[312+:3];
	assign fu_crossbar__prologue_count_inport[81+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[81+:3];
	assign fu_crossbar__prologue_count_inport[78+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[78+:3];
	assign routing_crossbar__prologue_count_inport[309+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[309+:3];
	assign routing_crossbar__prologue_count_inport[306+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[306+:3];
	assign routing_crossbar__prologue_count_inport[303+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[303+:3];
	assign routing_crossbar__prologue_count_inport[300+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[300+:3];
	assign routing_crossbar__prologue_count_inport[297+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[297+:3];
	assign routing_crossbar__prologue_count_inport[294+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[294+:3];
	assign routing_crossbar__prologue_count_inport[291+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[291+:3];
	assign routing_crossbar__prologue_count_inport[288+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[288+:3];
	assign fu_crossbar__prologue_count_inport[75+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[75+:3];
	assign fu_crossbar__prologue_count_inport[72+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[72+:3];
	assign routing_crossbar__prologue_count_inport[285+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[285+:3];
	assign routing_crossbar__prologue_count_inport[282+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[282+:3];
	assign routing_crossbar__prologue_count_inport[279+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[279+:3];
	assign routing_crossbar__prologue_count_inport[276+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[276+:3];
	assign routing_crossbar__prologue_count_inport[273+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[273+:3];
	assign routing_crossbar__prologue_count_inport[270+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[270+:3];
	assign routing_crossbar__prologue_count_inport[267+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[267+:3];
	assign routing_crossbar__prologue_count_inport[264+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[264+:3];
	assign fu_crossbar__prologue_count_inport[69+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[69+:3];
	assign fu_crossbar__prologue_count_inport[66+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[66+:3];
	assign routing_crossbar__prologue_count_inport[261+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[261+:3];
	assign routing_crossbar__prologue_count_inport[258+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[258+:3];
	assign routing_crossbar__prologue_count_inport[255+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[255+:3];
	assign routing_crossbar__prologue_count_inport[252+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[252+:3];
	assign routing_crossbar__prologue_count_inport[249+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[249+:3];
	assign routing_crossbar__prologue_count_inport[246+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[246+:3];
	assign routing_crossbar__prologue_count_inport[243+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[243+:3];
	assign routing_crossbar__prologue_count_inport[240+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[240+:3];
	assign fu_crossbar__prologue_count_inport[63+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[63+:3];
	assign fu_crossbar__prologue_count_inport[60+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[60+:3];
	assign routing_crossbar__prologue_count_inport[237+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[237+:3];
	assign routing_crossbar__prologue_count_inport[234+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[234+:3];
	assign routing_crossbar__prologue_count_inport[231+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[231+:3];
	assign routing_crossbar__prologue_count_inport[228+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[228+:3];
	assign routing_crossbar__prologue_count_inport[225+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[225+:3];
	assign routing_crossbar__prologue_count_inport[222+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[222+:3];
	assign routing_crossbar__prologue_count_inport[219+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[219+:3];
	assign routing_crossbar__prologue_count_inport[216+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[216+:3];
	assign fu_crossbar__prologue_count_inport[57+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[57+:3];
	assign fu_crossbar__prologue_count_inport[54+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[54+:3];
	assign routing_crossbar__prologue_count_inport[213+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[213+:3];
	assign routing_crossbar__prologue_count_inport[210+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[210+:3];
	assign routing_crossbar__prologue_count_inport[207+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[207+:3];
	assign routing_crossbar__prologue_count_inport[204+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[204+:3];
	assign routing_crossbar__prologue_count_inport[201+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[201+:3];
	assign routing_crossbar__prologue_count_inport[198+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[198+:3];
	assign routing_crossbar__prologue_count_inport[195+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[195+:3];
	assign routing_crossbar__prologue_count_inport[192+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[192+:3];
	assign fu_crossbar__prologue_count_inport[51+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[51+:3];
	assign fu_crossbar__prologue_count_inport[48+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[48+:3];
	assign routing_crossbar__prologue_count_inport[189+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[189+:3];
	assign routing_crossbar__prologue_count_inport[186+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[186+:3];
	assign routing_crossbar__prologue_count_inport[183+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[183+:3];
	assign routing_crossbar__prologue_count_inport[180+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[180+:3];
	assign routing_crossbar__prologue_count_inport[177+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[177+:3];
	assign routing_crossbar__prologue_count_inport[174+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[174+:3];
	assign routing_crossbar__prologue_count_inport[171+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[171+:3];
	assign routing_crossbar__prologue_count_inport[168+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[168+:3];
	assign fu_crossbar__prologue_count_inport[45+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[45+:3];
	assign fu_crossbar__prologue_count_inport[42+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[42+:3];
	assign routing_crossbar__prologue_count_inport[165+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[165+:3];
	assign routing_crossbar__prologue_count_inport[162+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[162+:3];
	assign routing_crossbar__prologue_count_inport[159+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[159+:3];
	assign routing_crossbar__prologue_count_inport[156+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[156+:3];
	assign routing_crossbar__prologue_count_inport[153+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[153+:3];
	assign routing_crossbar__prologue_count_inport[150+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[150+:3];
	assign routing_crossbar__prologue_count_inport[147+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[147+:3];
	assign routing_crossbar__prologue_count_inport[144+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[144+:3];
	assign fu_crossbar__prologue_count_inport[39+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[39+:3];
	assign fu_crossbar__prologue_count_inport[36+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[36+:3];
	assign routing_crossbar__prologue_count_inport[141+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[141+:3];
	assign routing_crossbar__prologue_count_inport[138+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[138+:3];
	assign routing_crossbar__prologue_count_inport[135+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[135+:3];
	assign routing_crossbar__prologue_count_inport[132+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[132+:3];
	assign routing_crossbar__prologue_count_inport[129+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[129+:3];
	assign routing_crossbar__prologue_count_inport[126+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[126+:3];
	assign routing_crossbar__prologue_count_inport[123+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[123+:3];
	assign routing_crossbar__prologue_count_inport[120+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[120+:3];
	assign fu_crossbar__prologue_count_inport[33+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[33+:3];
	assign fu_crossbar__prologue_count_inport[30+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[30+:3];
	assign routing_crossbar__prologue_count_inport[117+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[117+:3];
	assign routing_crossbar__prologue_count_inport[114+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[114+:3];
	assign routing_crossbar__prologue_count_inport[111+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[111+:3];
	assign routing_crossbar__prologue_count_inport[108+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[108+:3];
	assign routing_crossbar__prologue_count_inport[105+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[105+:3];
	assign routing_crossbar__prologue_count_inport[102+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[102+:3];
	assign routing_crossbar__prologue_count_inport[99+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[99+:3];
	assign routing_crossbar__prologue_count_inport[96+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[96+:3];
	assign fu_crossbar__prologue_count_inport[27+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[27+:3];
	assign fu_crossbar__prologue_count_inport[24+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[24+:3];
	assign routing_crossbar__prologue_count_inport[93+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[93+:3];
	assign routing_crossbar__prologue_count_inport[90+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[90+:3];
	assign routing_crossbar__prologue_count_inport[87+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[87+:3];
	assign routing_crossbar__prologue_count_inport[84+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[84+:3];
	assign routing_crossbar__prologue_count_inport[81+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[81+:3];
	assign routing_crossbar__prologue_count_inport[78+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[78+:3];
	assign routing_crossbar__prologue_count_inport[75+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[75+:3];
	assign routing_crossbar__prologue_count_inport[72+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[72+:3];
	assign fu_crossbar__prologue_count_inport[21+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[21+:3];
	assign fu_crossbar__prologue_count_inport[18+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[18+:3];
	assign routing_crossbar__prologue_count_inport[69+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[69+:3];
	assign routing_crossbar__prologue_count_inport[66+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[66+:3];
	assign routing_crossbar__prologue_count_inport[63+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[63+:3];
	assign routing_crossbar__prologue_count_inport[60+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[60+:3];
	assign routing_crossbar__prologue_count_inport[57+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[57+:3];
	assign routing_crossbar__prologue_count_inport[54+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[54+:3];
	assign routing_crossbar__prologue_count_inport[51+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[51+:3];
	assign routing_crossbar__prologue_count_inport[48+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[48+:3];
	assign fu_crossbar__prologue_count_inport[15+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[15+:3];
	assign fu_crossbar__prologue_count_inport[12+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[12+:3];
	assign routing_crossbar__prologue_count_inport[45+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[45+:3];
	assign routing_crossbar__prologue_count_inport[42+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[42+:3];
	assign routing_crossbar__prologue_count_inport[39+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[39+:3];
	assign routing_crossbar__prologue_count_inport[36+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[36+:3];
	assign routing_crossbar__prologue_count_inport[33+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[33+:3];
	assign routing_crossbar__prologue_count_inport[30+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[30+:3];
	assign routing_crossbar__prologue_count_inport[27+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[27+:3];
	assign routing_crossbar__prologue_count_inport[24+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[24+:3];
	assign fu_crossbar__prologue_count_inport[9+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[9+:3];
	assign fu_crossbar__prologue_count_inport[6+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[6+:3];
	assign routing_crossbar__prologue_count_inport[21+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[21+:3];
	assign routing_crossbar__prologue_count_inport[18+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[18+:3];
	assign routing_crossbar__prologue_count_inport[15+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[15+:3];
	assign routing_crossbar__prologue_count_inport[12+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[12+:3];
	assign routing_crossbar__prologue_count_inport[9+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[9+:3];
	assign routing_crossbar__prologue_count_inport[6+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[6+:3];
	assign routing_crossbar__prologue_count_inport[3+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[3+:3];
	assign routing_crossbar__prologue_count_inport[0+:3] = ctrl_mem__prologue_count_outport_routing_crossbar[0+:3];
	assign fu_crossbar__prologue_count_inport[3+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[3+:3];
	assign fu_crossbar__prologue_count_inport[0+:3] = ctrl_mem__prologue_count_outport_fu_crossbar[0+:3];
	assign element__to_mem_raddr__rdy[12+:1] = 1'd0;
	assign element__from_mem_rdata__val[12+:1] = 1'd0;
	assign element__from_mem_rdata__msg[420+:35] = 35'h000000000;
	assign element__to_mem_waddr__rdy[12+:1] = 1'd0;
	assign element__to_mem_wdata__rdy[12+:1] = 1'd0;
	assign element__to_mem_raddr__rdy[11+:1] = 1'd0;
	assign element__from_mem_rdata__val[11+:1] = 1'd0;
	assign element__from_mem_rdata__msg[385+:35] = 35'h000000000;
	assign element__to_mem_waddr__rdy[11+:1] = 1'd0;
	assign element__to_mem_wdata__rdy[11+:1] = 1'd0;
	assign element__to_mem_raddr__rdy[10+:1] = 1'd0;
	assign element__from_mem_rdata__val[10+:1] = 1'd0;
	assign element__from_mem_rdata__msg[350+:35] = 35'h000000000;
	assign element__to_mem_waddr__rdy[10+:1] = 1'd0;
	assign element__to_mem_wdata__rdy[10+:1] = 1'd0;
	assign element__to_mem_raddr__rdy[9+:1] = 1'd0;
	assign element__from_mem_rdata__val[9+:1] = 1'd0;
	assign element__from_mem_rdata__msg[315+:35] = 35'h000000000;
	assign element__to_mem_waddr__rdy[9+:1] = 1'd0;
	assign element__to_mem_wdata__rdy[9+:1] = 1'd0;
	assign element__to_mem_raddr__rdy[8+:1] = 1'd0;
	assign element__from_mem_rdata__val[8+:1] = 1'd0;
	assign element__from_mem_rdata__msg[280+:35] = 35'h000000000;
	assign element__to_mem_waddr__rdy[8+:1] = 1'd0;
	assign element__to_mem_wdata__rdy[8+:1] = 1'd0;
	assign element__to_mem_raddr__rdy[7+:1] = 1'd0;
	assign element__from_mem_rdata__val[7+:1] = 1'd0;
	assign element__from_mem_rdata__msg[245+:35] = 35'h000000000;
	assign element__to_mem_waddr__rdy[7+:1] = 1'd0;
	assign element__to_mem_wdata__rdy[7+:1] = 1'd0;
	assign element__to_mem_raddr__rdy[6+:1] = 1'd0;
	assign element__from_mem_rdata__val[6+:1] = 1'd0;
	assign element__from_mem_rdata__msg[210+:35] = 35'h000000000;
	assign element__to_mem_waddr__rdy[6+:1] = 1'd0;
	assign element__to_mem_wdata__rdy[6+:1] = 1'd0;
	assign element__to_mem_raddr__rdy[5+:1] = 1'd0;
	assign element__from_mem_rdata__val[5+:1] = 1'd0;
	assign element__from_mem_rdata__msg[175+:35] = 35'h000000000;
	assign element__to_mem_waddr__rdy[5+:1] = 1'd0;
	assign element__to_mem_wdata__rdy[5+:1] = 1'd0;
	assign element__to_mem_raddr__rdy[4+:1] = 1'd0;
	assign element__from_mem_rdata__val[4+:1] = 1'd0;
	assign element__from_mem_rdata__msg[140+:35] = 35'h000000000;
	assign element__to_mem_waddr__rdy[4+:1] = 1'd0;
	assign element__to_mem_wdata__rdy[4+:1] = 1'd0;
	assign to_mem_raddr__msg = element__to_mem_raddr__msg[45+:15];
	assign element__to_mem_raddr__rdy[3+:1] = to_mem_raddr__rdy;
	assign to_mem_raddr__val = element__to_mem_raddr__val[3+:1];
	assign element__from_mem_rdata__msg[105+:35] = from_mem_rdata__msg;
	assign from_mem_rdata__rdy = element__from_mem_rdata__rdy[3+:1];
	assign element__from_mem_rdata__val[3+:1] = from_mem_rdata__val;
	assign to_mem_waddr__msg = element__to_mem_waddr__msg[45+:15];
	assign element__to_mem_waddr__rdy[3+:1] = to_mem_waddr__rdy;
	assign to_mem_waddr__val = element__to_mem_waddr__val[3+:1];
	assign to_mem_wdata__msg = element__to_mem_wdata__msg[105+:35];
	assign element__to_mem_wdata__rdy[3+:1] = to_mem_wdata__rdy;
	assign to_mem_wdata__val = element__to_mem_wdata__val[3+:1];
	assign element__to_mem_raddr__rdy[2+:1] = 1'd0;
	assign element__from_mem_rdata__val[2+:1] = 1'd0;
	assign element__from_mem_rdata__msg[70+:35] = 35'h000000000;
	assign element__to_mem_waddr__rdy[2+:1] = 1'd0;
	assign element__to_mem_wdata__rdy[2+:1] = 1'd0;
	assign element__to_mem_raddr__rdy[1+:1] = 1'd0;
	assign element__from_mem_rdata__val[1+:1] = 1'd0;
	assign element__from_mem_rdata__msg[35+:35] = 35'h000000000;
	assign element__to_mem_waddr__rdy[1+:1] = 1'd0;
	assign element__to_mem_wdata__rdy[1+:1] = 1'd0;
	assign element__to_mem_raddr__rdy[0+:1] = 1'd0;
	assign element__from_mem_rdata__val[0+:1] = 1'd0;
	assign element__from_mem_rdata__msg[0+:35] = 35'h000000000;
	assign element__to_mem_waddr__rdy[0+:1] = 1'd0;
	assign element__to_mem_wdata__rdy[0+:1] = 1'd0;
	assign element__clear[12+:1] = ctrl_mem__rearm;
	assign element__clear[11+:1] = ctrl_mem__rearm;
	assign element__clear[10+:1] = clear;
	assign element__clear[9+:1] = ctrl_mem__rearm;
	assign element__clear[8+:1] = ctrl_mem__rearm;
	assign element__clear[7+:1] = ctrl_mem__rearm;
	assign element__clear[6+:1] = ctrl_mem__rearm;
	assign element__clear[5+:1] = ctrl_mem__rearm;
	assign element__clear[4+:1] = clear;
	assign element__clear[3+:1] = ctrl_mem__rearm;
	assign element__clear[2+:1] = clear;
	assign element__clear[1+:1] = ctrl_mem__rearm;
	assign element__clear[0+:1] = ctrl_mem__rearm;
	assign fu_crossbar__clear = clear;
	assign register_cluster__clear = clear;
	assign routing_crossbar__clear = clear;
	assign const_mem__clear = clear;
	assign tile_in_channel__recv__msg[0] = recv_data__msg[105+:35];
	assign routing_crossbar__recv_data__msg[245+:35] = tile_in_channel__send__msg[0];
	assign tile_in_channel__send__rdy[0] = routing_crossbar__recv_data__rdy[7+:1];
	assign routing_crossbar__recv_data__val[7+:1] = tile_in_channel__send__val[0];
	assign tile_in_channel__clear[0] = clear;
	assign tile_in_channel__recv__msg[1] = recv_data__msg[70+:35];
	assign routing_crossbar__recv_data__msg[210+:35] = tile_in_channel__send__msg[1];
	assign tile_in_channel__send__rdy[1] = routing_crossbar__recv_data__rdy[6+:1];
	assign routing_crossbar__recv_data__val[6+:1] = tile_in_channel__send__val[1];
	assign tile_in_channel__clear[1] = clear;
	assign tile_in_channel__recv__msg[2] = recv_data__msg[35+:35];
	assign routing_crossbar__recv_data__msg[175+:35] = tile_in_channel__send__msg[2];
	assign tile_in_channel__send__rdy[2] = routing_crossbar__recv_data__rdy[5+:1];
	assign routing_crossbar__recv_data__val[5+:1] = tile_in_channel__send__val[2];
	assign tile_in_channel__clear[2] = clear;
	assign tile_in_channel__recv__msg[3] = recv_data__msg[0+:35];
	assign routing_crossbar__recv_data__msg[140+:35] = tile_in_channel__send__msg[3];
	assign tile_in_channel__send__rdy[3] = routing_crossbar__recv_data__rdy[4+:1];
	assign routing_crossbar__recv_data__val[4+:1] = tile_in_channel__send__val[3];
	assign tile_in_channel__clear[3] = clear;
	assign routing_crossbar__recv_data__msg[105+:35] = register_cluster__send_data_to_routing_crossbar__msg[105+:35];
	assign register_cluster__send_data_to_routing_crossbar__rdy[3+:1] = routing_crossbar__recv_data__rdy[3+:1];
	assign routing_crossbar__recv_data__val[3+:1] = register_cluster__send_data_to_routing_crossbar__val[3+:1];
	assign routing_crossbar__recv_data__msg[70+:35] = register_cluster__send_data_to_routing_crossbar__msg[70+:35];
	assign register_cluster__send_data_to_routing_crossbar__rdy[2+:1] = routing_crossbar__recv_data__rdy[2+:1];
	assign routing_crossbar__recv_data__val[2+:1] = register_cluster__send_data_to_routing_crossbar__val[2+:1];
	assign routing_crossbar__recv_data__msg[35+:35] = register_cluster__send_data_to_routing_crossbar__msg[35+:35];
	assign register_cluster__send_data_to_routing_crossbar__rdy[1+:1] = routing_crossbar__recv_data__rdy[1+:1];
	assign routing_crossbar__recv_data__val[1+:1] = register_cluster__send_data_to_routing_crossbar__val[1+:1];
	assign routing_crossbar__recv_data__msg[0+:35] = register_cluster__send_data_to_routing_crossbar__msg[0+:35];
	assign register_cluster__send_data_to_routing_crossbar__rdy[0+:1] = routing_crossbar__recv_data__rdy[0+:1];
	assign routing_crossbar__recv_data__val[0+:1] = register_cluster__send_data_to_routing_crossbar__val[0+:1];
	assign routing_crossbar__crossbar_outport[28+:4] = ctrl_mem__send_ctrl__msg[60+:4];
	assign fu_crossbar__crossbar_outport[14+:2] = ctrl_mem__send_ctrl__msg[44+:2];
	assign routing_crossbar__crossbar_outport[24+:4] = ctrl_mem__send_ctrl__msg[64+:4];
	assign fu_crossbar__crossbar_outport[12+:2] = ctrl_mem__send_ctrl__msg[46+:2];
	assign routing_crossbar__crossbar_outport[20+:4] = ctrl_mem__send_ctrl__msg[68+:4];
	assign fu_crossbar__crossbar_outport[10+:2] = ctrl_mem__send_ctrl__msg[48+:2];
	assign routing_crossbar__crossbar_outport[16+:4] = ctrl_mem__send_ctrl__msg[72+:4];
	assign fu_crossbar__crossbar_outport[8+:2] = ctrl_mem__send_ctrl__msg[50+:2];
	assign routing_crossbar__crossbar_outport[12+:4] = ctrl_mem__send_ctrl__msg[76+:4];
	assign fu_crossbar__crossbar_outport[6+:2] = ctrl_mem__send_ctrl__msg[52+:2];
	assign routing_crossbar__crossbar_outport[8+:4] = ctrl_mem__send_ctrl__msg[80+:4];
	assign fu_crossbar__crossbar_outport[4+:2] = ctrl_mem__send_ctrl__msg[54+:2];
	assign routing_crossbar__crossbar_outport[4+:4] = ctrl_mem__send_ctrl__msg[84+:4];
	assign fu_crossbar__crossbar_outport[2+:2] = ctrl_mem__send_ctrl__msg[56+:2];
	assign routing_crossbar__crossbar_outport[0+:4] = ctrl_mem__send_ctrl__msg[88+:4];
	assign fu_crossbar__crossbar_outport[0+:2] = ctrl_mem__send_ctrl__msg[58+:2];
	assign fu_crossbar__recv_data__msg[0+:35] = element__send_out__msg[0+:35];
	assign element__send_out__rdy[0+:1] = fu_crossbar__recv_data__rdy[0+:1];
	assign fu_crossbar__recv_data__val[0+:1] = element__send_out__val[0+:1];
	assign tile_out_or_link__recv_fu__msg[0] = fu_crossbar__send_data__msg[245+:35];
	assign fu_crossbar__send_data__rdy[7+:1] = tile_out_or_link__recv_fu__rdy[0];
	assign tile_out_or_link__recv_fu__val[0] = fu_crossbar__send_data__val[7+:1];
	assign tile_out_or_link__recv_xbar__msg[0] = routing_crossbar__send_data__msg[245+:35];
	assign routing_crossbar__send_data__rdy[7+:1] = tile_out_or_link__recv_xbar__rdy[0];
	assign tile_out_or_link__recv_xbar__val[0] = routing_crossbar__send_data__val[7+:1];
	assign send_data__msg[105+:35] = tile_out_or_link__send__msg[0];
	assign tile_out_or_link__send__rdy[0] = send_data__rdy[3+:1];
	assign send_data__val[3+:1] = tile_out_or_link__send__val[0];
	assign tile_out_or_link__recv_fu__msg[1] = fu_crossbar__send_data__msg[210+:35];
	assign fu_crossbar__send_data__rdy[6+:1] = tile_out_or_link__recv_fu__rdy[1];
	assign tile_out_or_link__recv_fu__val[1] = fu_crossbar__send_data__val[6+:1];
	assign tile_out_or_link__recv_xbar__msg[1] = routing_crossbar__send_data__msg[210+:35];
	assign routing_crossbar__send_data__rdy[6+:1] = tile_out_or_link__recv_xbar__rdy[1];
	assign tile_out_or_link__recv_xbar__val[1] = routing_crossbar__send_data__val[6+:1];
	assign send_data__msg[70+:35] = tile_out_or_link__send__msg[1];
	assign tile_out_or_link__send__rdy[1] = send_data__rdy[2+:1];
	assign send_data__val[2+:1] = tile_out_or_link__send__val[1];
	assign tile_out_or_link__recv_fu__msg[2] = fu_crossbar__send_data__msg[175+:35];
	assign fu_crossbar__send_data__rdy[5+:1] = tile_out_or_link__recv_fu__rdy[2];
	assign tile_out_or_link__recv_fu__val[2] = fu_crossbar__send_data__val[5+:1];
	assign tile_out_or_link__recv_xbar__msg[2] = routing_crossbar__send_data__msg[175+:35];
	assign routing_crossbar__send_data__rdy[5+:1] = tile_out_or_link__recv_xbar__rdy[2];
	assign tile_out_or_link__recv_xbar__val[2] = routing_crossbar__send_data__val[5+:1];
	assign send_data__msg[35+:35] = tile_out_or_link__send__msg[2];
	assign tile_out_or_link__send__rdy[2] = send_data__rdy[1+:1];
	assign send_data__val[1+:1] = tile_out_or_link__send__val[2];
	assign tile_out_or_link__recv_fu__msg[3] = fu_crossbar__send_data__msg[140+:35];
	assign fu_crossbar__send_data__rdy[4+:1] = tile_out_or_link__recv_fu__rdy[3];
	assign tile_out_or_link__recv_fu__val[3] = fu_crossbar__send_data__val[4+:1];
	assign tile_out_or_link__recv_xbar__msg[3] = routing_crossbar__send_data__msg[140+:35];
	assign routing_crossbar__send_data__rdy[4+:1] = tile_out_or_link__recv_xbar__rdy[3];
	assign tile_out_or_link__recv_xbar__val[3] = routing_crossbar__send_data__val[4+:1];
	assign send_data__msg[0+:35] = tile_out_or_link__send__msg[3];
	assign tile_out_or_link__send__rdy[3] = send_data__rdy[0+:1];
	assign send_data__val[0+:1] = tile_out_or_link__send__val[3];
	assign register_cluster__recv_data_from_routing_crossbar__msg[105+:35] = routing_crossbar__send_data__msg[105+:35];
	assign routing_crossbar__send_data__rdy[3+:1] = register_cluster__recv_data_from_routing_crossbar__rdy[3+:1];
	assign register_cluster__recv_data_from_routing_crossbar__val[3+:1] = routing_crossbar__send_data__val[3+:1];
	assign register_cluster__recv_data_from_fu_crossbar__msg[105+:35] = fu_crossbar__send_data__msg[105+:35];
	assign fu_crossbar__send_data__rdy[3+:1] = register_cluster__recv_data_from_fu_crossbar__rdy[3+:1];
	assign register_cluster__recv_data_from_fu_crossbar__val[3+:1] = fu_crossbar__send_data__val[3+:1];
	assign register_cluster__recv_data_from_const__msg[105+:35] = 35'h000000000;
	assign register_cluster__recv_data_from_const__val[3+:1] = 1'd0;
	assign element__recv_in__msg[105+:35] = register_cluster__send_data_to_fu__msg[105+:35];
	assign register_cluster__send_data_to_fu__rdy[3+:1] = element__recv_in__rdy[3+:1];
	assign element__recv_in__val[3+:1] = register_cluster__send_data_to_fu__val[3+:1];
	assign register_cluster__inport_opt = ctrl_mem__send_ctrl__msg;
	assign register_cluster__recv_data_from_routing_crossbar__msg[70+:35] = routing_crossbar__send_data__msg[70+:35];
	assign routing_crossbar__send_data__rdy[2+:1] = register_cluster__recv_data_from_routing_crossbar__rdy[2+:1];
	assign register_cluster__recv_data_from_routing_crossbar__val[2+:1] = routing_crossbar__send_data__val[2+:1];
	assign register_cluster__recv_data_from_fu_crossbar__msg[70+:35] = fu_crossbar__send_data__msg[70+:35];
	assign fu_crossbar__send_data__rdy[2+:1] = register_cluster__recv_data_from_fu_crossbar__rdy[2+:1];
	assign register_cluster__recv_data_from_fu_crossbar__val[2+:1] = fu_crossbar__send_data__val[2+:1];
	assign register_cluster__recv_data_from_const__msg[70+:35] = 35'h000000000;
	assign register_cluster__recv_data_from_const__val[2+:1] = 1'd0;
	assign element__recv_in__msg[70+:35] = register_cluster__send_data_to_fu__msg[70+:35];
	assign register_cluster__send_data_to_fu__rdy[2+:1] = element__recv_in__rdy[2+:1];
	assign element__recv_in__val[2+:1] = register_cluster__send_data_to_fu__val[2+:1];
	assign register_cluster__recv_data_from_routing_crossbar__msg[35+:35] = routing_crossbar__send_data__msg[35+:35];
	assign routing_crossbar__send_data__rdy[1+:1] = register_cluster__recv_data_from_routing_crossbar__rdy[1+:1];
	assign register_cluster__recv_data_from_routing_crossbar__val[1+:1] = routing_crossbar__send_data__val[1+:1];
	assign register_cluster__recv_data_from_fu_crossbar__msg[35+:35] = fu_crossbar__send_data__msg[35+:35];
	assign fu_crossbar__send_data__rdy[1+:1] = register_cluster__recv_data_from_fu_crossbar__rdy[1+:1];
	assign register_cluster__recv_data_from_fu_crossbar__val[1+:1] = fu_crossbar__send_data__val[1+:1];
	assign register_cluster__recv_data_from_const__msg[35+:35] = 35'h000000000;
	assign register_cluster__recv_data_from_const__val[1+:1] = 1'd0;
	assign element__recv_in__msg[35+:35] = register_cluster__send_data_to_fu__msg[35+:35];
	assign register_cluster__send_data_to_fu__rdy[1+:1] = element__recv_in__rdy[1+:1];
	assign element__recv_in__val[1+:1] = register_cluster__send_data_to_fu__val[1+:1];
	assign register_cluster__recv_data_from_routing_crossbar__msg[0+:35] = routing_crossbar__send_data__msg[0+:35];
	assign routing_crossbar__send_data__rdy[0+:1] = register_cluster__recv_data_from_routing_crossbar__rdy[0+:1];
	assign register_cluster__recv_data_from_routing_crossbar__val[0+:1] = routing_crossbar__send_data__val[0+:1];
	assign register_cluster__recv_data_from_fu_crossbar__msg[0+:35] = fu_crossbar__send_data__msg[0+:35];
	assign fu_crossbar__send_data__rdy[0+:1] = register_cluster__recv_data_from_fu_crossbar__rdy[0+:1];
	assign register_cluster__recv_data_from_fu_crossbar__val[0+:1] = fu_crossbar__send_data__val[0+:1];
	assign register_cluster__recv_data_from_const__msg[0+:35] = 35'h000000000;
	assign register_cluster__recv_data_from_const__val[0+:1] = 1'd0;
	assign element__recv_in__msg[0+:35] = register_cluster__send_data_to_fu__msg[0+:35];
	assign register_cluster__send_data_to_fu__rdy[0+:1] = element__recv_in__rdy[0+:1];
	assign element__recv_in__val[0+:1] = register_cluster__send_data_to_fu__val[0+:1];
	assign fu_crossbar__recv_data__msg[35+:35] = element__send_out__msg[35+:35];
	assign element__send_out__rdy[1+:1] = fu_crossbar__recv_data__rdy[1+:1];
	assign fu_crossbar__recv_data__val[1+:1] = element__send_out__val[1+:1];
	initial _sv2v_0 = 0;
endmodule
module PeQueue_NormalQueueWithClearRTL__17e45bee21dfb909 (
	clear,
	clk,
	count,
	reset,
	recv__msg,
	recv__rdy,
	recv__val,
	send__msg,
	send__rdy,
	send__val
);
	input wire [0:0] clear;
	input wire [0:0] clk;
	output wire [1:0] count;
	input wire [0:0] reset;
	input wire [172:0] recv__msg;
	output wire [0:0] recv__rdy;
	input wire [0:0] recv__val;
	output reg [172:0] send__msg;
	input wire [0:0] send__rdy;
	output wire [0:0] send__val;
	reg full;
	assign recv__rdy = !full;
	assign send__val = full;
	assign count = {1'b0, full};
	always @(posedge clk) begin
		if (reset | clear)
			full <= 0;
		else if (recv__val && recv__rdy)
			full <= 1;
		else if (send__val && send__rdy)
			full <= 0;
		if (recv__val && recv__rdy)
			send__msg <= recv__msg;
	end
endmodule
module PeQueue_NormalQueueRTL__3f50370143e38621 (
	clk,
	count,
	reset,
	recv__msg,
	recv__rdy,
	recv__val,
	send__msg,
	send__rdy,
	send__val
);
	input wire [0:0] clk;
	output wire [1:0] count;
	input wire [0:0] reset;
	input wire [197:0] recv__msg;
	output wire [0:0] recv__rdy;
	input wire [0:0] recv__val;
	output reg [197:0] send__msg;
	input wire [0:0] send__rdy;
	output wire [0:0] send__val;
	reg full;
	assign recv__rdy = !full;
	assign send__val = full;
	assign count = {1'b0, full};
	always @(posedge clk) begin
		if (reset)
			full <= 0;
		else if (recv__val && recv__rdy)
			full <= 1;
		else if (send__val && send__rdy)
			full <= 0;
		if (recv__val && recv__rdy)
			send__msg <= recv__msg;
	end
endmodule
module PeQueue_RegisterFile__82f517189341f536 (
	clk,
	raddr,
	rdata,
	reset,
	waddr,
	wdata,
	wen
);
	reg _sv2v_0;
	input wire [0:0] clk;
	input wire [5:0] raddr;
	output reg [110:0] rdata;
	input wire [0:0] reset;
	input wire [5:0] waddr;
	input wire [110:0] wdata;
	input wire [0:0] wen;
	localparam [0:0] __const__rd_ports_at_up_rf_read = 1'd1;
	localparam [0:0] __const__wr_ports_at_up_rf_write = 1'd1;
	reg [110:0] regs [0:47];
	function automatic [0:0] sv2v_cast_1;
		input reg [0:0] inp;
		sv2v_cast_1 = inp;
	endfunction
	always @(*) begin : up_rf_read
		if (_sv2v_0)
			;
		begin : sv2v_autoblock_1
			reg [31:0] i;
			for (i = 1'd0; i < __const__rd_ports_at_up_rf_read; i = i + 1'd1)
				rdata[sv2v_cast_1(i) * 111+:111] = regs[raddr[sv2v_cast_1(i) * 6+:6]];
		end
	end
	always @(posedge clk) begin : up_rf_write
		begin : sv2v_autoblock_2
			reg [31:0] i;
			for (i = 1'd0; i < __const__wr_ports_at_up_rf_write; i = i + 1'd1)
				if (wen[sv2v_cast_1(i)+:1])
					regs[waddr[sv2v_cast_1(i) * 6+:6]] <= wdata[sv2v_cast_1(i) * 111+:111];
		end
	end
	initial _sv2v_0 = 0;
endmodule
module CgraCompute (
	address_lower,
	address_upper,
	cgra_id,
	clk,
	reset,
	dma_cmd__msg,
	dma_cmd__rdy,
	dma_cmd__val,
	dma_done__msg,
	dma_done__rdy,
	dma_done__val,
	recv_from_cpu_pkt__msg,
	recv_from_cpu_pkt__rdy,
	recv_from_cpu_pkt__val,
	recv_from_dma_spm_rd_req__msg,
	recv_from_dma_spm_rd_req__rdy,
	recv_from_dma_spm_rd_req__val,
	recv_from_dma_spm_wr_req__msg,
	recv_from_dma_spm_wr_req__rdy,
	recv_from_dma_spm_wr_req__val,
	recv_from_inter_cgra_noc__msg,
	recv_from_inter_cgra_noc__rdy,
	recv_from_inter_cgra_noc__val,
	send_to_cpu_pkt__msg,
	send_to_cpu_pkt__rdy,
	send_to_cpu_pkt__val,
	send_to_dma_spm_rd_resp__msg,
	send_to_dma_spm_rd_resp__rdy,
	send_to_dma_spm_rd_resp__val,
	send_to_inter_cgra_noc__msg,
	send_to_inter_cgra_noc__rdy,
	send_to_inter_cgra_noc__val,
	data_mem__address_lower,
	data_mem__address_upper,
	data_mem__cgra_id,
	data_mem__clk,
	data_mem__reset,
	data_mem__recv_from_controller_spm_rd_req__msg,
	data_mem__recv_from_controller_spm_rd_req__rdy,
	data_mem__recv_from_controller_spm_rd_req__val,
	data_mem__recv_from_controller_spm_wr_req__msg,
	data_mem__recv_from_controller_spm_wr_req__rdy,
	data_mem__recv_from_controller_spm_wr_req__val,
	data_mem__recv_from_noc_load_request__msg,
	data_mem__recv_from_noc_load_request__rdy,
	data_mem__recv_from_noc_load_request__val,
	data_mem__recv_from_noc_load_response_pkt__msg,
	data_mem__recv_from_noc_load_response_pkt__rdy,
	data_mem__recv_from_noc_load_response_pkt__val,
	data_mem__recv_from_noc_store_request__msg,
	data_mem__recv_from_noc_store_request__rdy,
	data_mem__recv_from_noc_store_request__val,
	data_mem__recv_raddr__msg,
	data_mem__recv_raddr__rdy,
	data_mem__recv_raddr__val,
	data_mem__recv_waddr__msg,
	data_mem__recv_waddr__rdy,
	data_mem__recv_waddr__val,
	data_mem__recv_wdata__msg,
	data_mem__recv_wdata__rdy,
	data_mem__recv_wdata__val,
	data_mem__send_rdata__msg,
	data_mem__send_rdata__rdy,
	data_mem__send_rdata__val,
	data_mem__send_to_controller_spm_rd_resp__msg,
	data_mem__send_to_controller_spm_rd_resp__rdy,
	data_mem__send_to_controller_spm_rd_resp__val,
	data_mem__send_to_noc_load_request_pkt__msg,
	data_mem__send_to_noc_load_request_pkt__rdy,
	data_mem__send_to_noc_load_request_pkt__val,
	data_mem__send_to_noc_load_response_pkt__msg,
	data_mem__send_to_noc_load_response_pkt__rdy,
	data_mem__send_to_noc_load_response_pkt__val,
	data_mem__send_to_noc_store_pkt__msg,
	data_mem__send_to_noc_store_pkt__rdy,
	data_mem__send_to_noc_store_pkt__val
);
	input wire [14:0] address_lower;
	input wire [14:0] address_upper;
	input wire [0:0] cgra_id;
	input wire [0:0] clk;
	input wire [0:0] reset;
	output wire [138:0] dma_cmd__msg;
	input wire [0:0] dma_cmd__rdy;
	output wire [0:0] dma_cmd__val;
	input wire [7:0] dma_done__msg;
	output wire [0:0] dma_done__rdy;
	input wire [0:0] dma_done__val;
	input wire [197:0] recv_from_cpu_pkt__msg;
	output wire [0:0] recv_from_cpu_pkt__rdy;
	input wire [0:0] recv_from_cpu_pkt__val;
	input wire [31:0] recv_from_dma_spm_rd_req__msg;
	output wire [0:0] recv_from_dma_spm_rd_req__rdy;
	input wire [0:0] recv_from_dma_spm_rd_req__val;
	input wire [67:0] recv_from_dma_spm_wr_req__msg;
	output wire [0:0] recv_from_dma_spm_wr_req__rdy;
	input wire [0:0] recv_from_dma_spm_wr_req__val;
	input wire [201:0] recv_from_inter_cgra_noc__msg;
	output wire [0:0] recv_from_inter_cgra_noc__rdy;
	input wire [0:0] recv_from_inter_cgra_noc__val;
	output wire [197:0] send_to_cpu_pkt__msg;
	input wire [0:0] send_to_cpu_pkt__rdy;
	output wire [0:0] send_to_cpu_pkt__val;
	output wire [31:0] send_to_dma_spm_rd_resp__msg;
	input wire [0:0] send_to_dma_spm_rd_resp__rdy;
	output wire [0:0] send_to_dma_spm_rd_resp__val;
	output wire [201:0] send_to_inter_cgra_noc__msg;
	input wire [0:0] send_to_inter_cgra_noc__rdy;
	output wire [0:0] send_to_inter_cgra_noc__val;
	output wire [14:0] data_mem__address_lower;
	output wire [14:0] data_mem__address_upper;
	output wire [0:0] data_mem__cgra_id;
	output wire [0:0] data_mem__clk;
	output wire [0:0] data_mem__reset;
	output wire [31:0] data_mem__recv_from_controller_spm_rd_req__msg;
	input wire [0:0] data_mem__recv_from_controller_spm_rd_req__rdy;
	output wire [0:0] data_mem__recv_from_controller_spm_rd_req__val;
	output wire [67:0] data_mem__recv_from_controller_spm_wr_req__msg;
	input wire [0:0] data_mem__recv_from_controller_spm_wr_req__rdy;
	output wire [0:0] data_mem__recv_from_controller_spm_wr_req__val;
	output wire [201:0] data_mem__recv_from_noc_load_request__msg;
	input wire [0:0] data_mem__recv_from_noc_load_request__rdy;
	output wire [0:0] data_mem__recv_from_noc_load_request__val;
	output wire [201:0] data_mem__recv_from_noc_load_response_pkt__msg;
	input wire [0:0] data_mem__recv_from_noc_load_response_pkt__rdy;
	output wire [0:0] data_mem__recv_from_noc_load_response_pkt__val;
	output wire [201:0] data_mem__recv_from_noc_store_request__msg;
	input wire [0:0] data_mem__recv_from_noc_store_request__rdy;
	output wire [0:0] data_mem__recv_from_noc_store_request__val;
	output wire [104:0] data_mem__recv_raddr__msg;
	input wire [6:0] data_mem__recv_raddr__rdy;
	output wire [6:0] data_mem__recv_raddr__val;
	output wire [104:0] data_mem__recv_waddr__msg;
	input wire [6:0] data_mem__recv_waddr__rdy;
	output wire [6:0] data_mem__recv_waddr__val;
	output wire [244:0] data_mem__recv_wdata__msg;
	input wire [6:0] data_mem__recv_wdata__rdy;
	output wire [6:0] data_mem__recv_wdata__val;
	input wire [244:0] data_mem__send_rdata__msg;
	output wire [6:0] data_mem__send_rdata__rdy;
	input wire [6:0] data_mem__send_rdata__val;
	input wire [31:0] data_mem__send_to_controller_spm_rd_resp__msg;
	output wire [0:0] data_mem__send_to_controller_spm_rd_resp__rdy;
	input wire [0:0] data_mem__send_to_controller_spm_rd_resp__val;
	input wire [201:0] data_mem__send_to_noc_load_request_pkt__msg;
	output wire [0:0] data_mem__send_to_noc_load_request_pkt__rdy;
	input wire [0:0] data_mem__send_to_noc_load_request_pkt__val;
	input wire [201:0] data_mem__send_to_noc_load_response_pkt__msg;
	output wire [0:0] data_mem__send_to_noc_load_response_pkt__rdy;
	input wire [0:0] data_mem__send_to_noc_load_response_pkt__val;
	input wire [201:0] data_mem__send_to_noc_store_pkt__msg;
	output wire [0:0] data_mem__send_to_noc_store_pkt__rdy;
	input wire [0:0] data_mem__send_to_noc_store_pkt__val;
	wire [0:0] bypass_queue__clk;
	wire [0:0] bypass_queue__count;
	wire [0:0] bypass_queue__reset;
	wire [201:0] bypass_queue__recv__msg;
	wire [0:0] bypass_queue__recv__rdy;
	wire [0:0] bypass_queue__recv__val;
	wire [201:0] bypass_queue__send__msg;
	wire [0:0] bypass_queue__send__rdy;
	wire [0:0] bypass_queue__send__val;
	BypassQueueRTL__de7a0309a59fe496 bypass_queue(
		.clk(bypass_queue__clk),
		.count(bypass_queue__count),
		.reset(bypass_queue__reset),
		.recv__msg(bypass_queue__recv__msg),
		.recv__rdy(bypass_queue__recv__rdy),
		.recv__val(bypass_queue__recv__val),
		.send__msg(bypass_queue__send__msg),
		.send__rdy(bypass_queue__send__rdy),
		.send__val(bypass_queue__send__val)
	);
	wire [0:0] controller__cgra_id;
	wire [0:0] controller__clk;
	wire [0:0] controller__reset;
	wire [138:0] controller__dma_cmd__msg;
	wire [0:0] controller__dma_cmd__rdy;
	wire [0:0] controller__dma_cmd__val;
	wire [7:0] controller__dma_done__msg;
	wire [0:0] controller__dma_done__rdy;
	wire [0:0] controller__dma_done__val;
	wire [197:0] controller__recv_from_cpu_pkt__msg;
	wire [0:0] controller__recv_from_cpu_pkt__rdy;
	wire [0:0] controller__recv_from_cpu_pkt__val;
	wire [197:0] controller__recv_from_ctrl_ring_pkt__msg;
	wire [0:0] controller__recv_from_ctrl_ring_pkt__rdy;
	wire [0:0] controller__recv_from_ctrl_ring_pkt__val;
	wire [31:0] controller__recv_from_dma_spm_rd_req__msg;
	wire [0:0] controller__recv_from_dma_spm_rd_req__rdy;
	wire [0:0] controller__recv_from_dma_spm_rd_req__val;
	wire [67:0] controller__recv_from_dma_spm_wr_req__msg;
	wire [0:0] controller__recv_from_dma_spm_wr_req__rdy;
	wire [0:0] controller__recv_from_dma_spm_wr_req__val;
	wire [197:0] controller__recv_from_im2col_pkt__msg;
	wire [0:0] controller__recv_from_im2col_pkt__rdy;
	wire [0:0] controller__recv_from_im2col_pkt__val;
	wire [201:0] controller__recv_from_inter_cgra_noc__msg;
	wire [0:0] controller__recv_from_inter_cgra_noc__rdy;
	wire [0:0] controller__recv_from_inter_cgra_noc__val;
	wire [31:0] controller__recv_from_sram_load_response__msg;
	wire [0:0] controller__recv_from_sram_load_response__rdy;
	wire [0:0] controller__recv_from_sram_load_response__val;
	wire [201:0] controller__recv_from_tile_load_request_pkt__msg;
	wire [0:0] controller__recv_from_tile_load_request_pkt__rdy;
	wire [0:0] controller__recv_from_tile_load_request_pkt__val;
	wire [201:0] controller__recv_from_tile_load_response_pkt__msg;
	wire [0:0] controller__recv_from_tile_load_response_pkt__rdy;
	wire [0:0] controller__recv_from_tile_load_response_pkt__val;
	wire [201:0] controller__recv_from_tile_store_request_pkt__msg;
	wire [0:0] controller__recv_from_tile_store_request_pkt__rdy;
	wire [0:0] controller__recv_from_tile_store_request_pkt__val;
	wire [197:0] controller__send_to_cpu_pkt__msg;
	wire [0:0] controller__send_to_cpu_pkt__rdy;
	wire [0:0] controller__send_to_cpu_pkt__val;
	wire [197:0] controller__send_to_ctrl_ring_pkt__msg;
	wire [0:0] controller__send_to_ctrl_ring_pkt__rdy;
	wire [0:0] controller__send_to_ctrl_ring_pkt__val;
	wire [31:0] controller__send_to_dma_spm_rd_resp__msg;
	wire [0:0] controller__send_to_dma_spm_rd_resp__rdy;
	wire [0:0] controller__send_to_dma_spm_rd_resp__val;
	wire [197:0] controller__send_to_im2col_engine_pkt__msg;
	wire [0:0] controller__send_to_im2col_engine_pkt__rdy;
	wire [0:0] controller__send_to_im2col_engine_pkt__val;
	wire [201:0] controller__send_to_inter_cgra_noc__msg;
	wire [0:0] controller__send_to_inter_cgra_noc__rdy;
	wire [0:0] controller__send_to_inter_cgra_noc__val;
	wire [31:0] controller__send_to_sram_load_request_from_dma__msg;
	wire [0:0] controller__send_to_sram_load_request_from_dma__rdy;
	wire [0:0] controller__send_to_sram_load_request_from_dma__val;
	wire [201:0] controller__send_to_sram_load_request_from_noc__msg;
	wire [0:0] controller__send_to_sram_load_request_from_noc__rdy;
	wire [0:0] controller__send_to_sram_load_request_from_noc__val;
	wire [67:0] controller__send_to_sram_store_request_from_dma__msg;
	wire [0:0] controller__send_to_sram_store_request_from_dma__rdy;
	wire [0:0] controller__send_to_sram_store_request_from_dma__val;
	wire [201:0] controller__send_to_sram_store_request_from_noc__msg;
	wire [0:0] controller__send_to_sram_store_request_from_noc__rdy;
	wire [0:0] controller__send_to_sram_store_request_from_noc__val;
	wire [201:0] controller__send_to_tile_load_response__msg;
	wire [0:0] controller__send_to_tile_load_response__rdy;
	wire [0:0] controller__send_to_tile_load_response__val;
	ControllerRTL__d74885762e9d302f controller(
		.cgra_id(controller__cgra_id),
		.clk(controller__clk),
		.reset(controller__reset),
		.dma_cmd__msg(controller__dma_cmd__msg),
		.dma_cmd__rdy(controller__dma_cmd__rdy),
		.dma_cmd__val(controller__dma_cmd__val),
		.dma_done__msg(controller__dma_done__msg),
		.dma_done__rdy(controller__dma_done__rdy),
		.dma_done__val(controller__dma_done__val),
		.recv_from_cpu_pkt__msg(controller__recv_from_cpu_pkt__msg),
		.recv_from_cpu_pkt__rdy(controller__recv_from_cpu_pkt__rdy),
		.recv_from_cpu_pkt__val(controller__recv_from_cpu_pkt__val),
		.recv_from_ctrl_ring_pkt__msg(controller__recv_from_ctrl_ring_pkt__msg),
		.recv_from_ctrl_ring_pkt__rdy(controller__recv_from_ctrl_ring_pkt__rdy),
		.recv_from_ctrl_ring_pkt__val(controller__recv_from_ctrl_ring_pkt__val),
		.recv_from_dma_spm_rd_req__msg(controller__recv_from_dma_spm_rd_req__msg),
		.recv_from_dma_spm_rd_req__rdy(controller__recv_from_dma_spm_rd_req__rdy),
		.recv_from_dma_spm_rd_req__val(controller__recv_from_dma_spm_rd_req__val),
		.recv_from_dma_spm_wr_req__msg(controller__recv_from_dma_spm_wr_req__msg),
		.recv_from_dma_spm_wr_req__rdy(controller__recv_from_dma_spm_wr_req__rdy),
		.recv_from_dma_spm_wr_req__val(controller__recv_from_dma_spm_wr_req__val),
		.recv_from_im2col_pkt__msg(controller__recv_from_im2col_pkt__msg),
		.recv_from_im2col_pkt__rdy(controller__recv_from_im2col_pkt__rdy),
		.recv_from_im2col_pkt__val(controller__recv_from_im2col_pkt__val),
		.recv_from_inter_cgra_noc__msg(controller__recv_from_inter_cgra_noc__msg),
		.recv_from_inter_cgra_noc__rdy(controller__recv_from_inter_cgra_noc__rdy),
		.recv_from_inter_cgra_noc__val(controller__recv_from_inter_cgra_noc__val),
		.recv_from_sram_load_response__msg(controller__recv_from_sram_load_response__msg),
		.recv_from_sram_load_response__rdy(controller__recv_from_sram_load_response__rdy),
		.recv_from_sram_load_response__val(controller__recv_from_sram_load_response__val),
		.recv_from_tile_load_request_pkt__msg(controller__recv_from_tile_load_request_pkt__msg),
		.recv_from_tile_load_request_pkt__rdy(controller__recv_from_tile_load_request_pkt__rdy),
		.recv_from_tile_load_request_pkt__val(controller__recv_from_tile_load_request_pkt__val),
		.recv_from_tile_load_response_pkt__msg(controller__recv_from_tile_load_response_pkt__msg),
		.recv_from_tile_load_response_pkt__rdy(controller__recv_from_tile_load_response_pkt__rdy),
		.recv_from_tile_load_response_pkt__val(controller__recv_from_tile_load_response_pkt__val),
		.recv_from_tile_store_request_pkt__msg(controller__recv_from_tile_store_request_pkt__msg),
		.recv_from_tile_store_request_pkt__rdy(controller__recv_from_tile_store_request_pkt__rdy),
		.recv_from_tile_store_request_pkt__val(controller__recv_from_tile_store_request_pkt__val),
		.send_to_cpu_pkt__msg(controller__send_to_cpu_pkt__msg),
		.send_to_cpu_pkt__rdy(controller__send_to_cpu_pkt__rdy),
		.send_to_cpu_pkt__val(controller__send_to_cpu_pkt__val),
		.send_to_ctrl_ring_pkt__msg(controller__send_to_ctrl_ring_pkt__msg),
		.send_to_ctrl_ring_pkt__rdy(controller__send_to_ctrl_ring_pkt__rdy),
		.send_to_ctrl_ring_pkt__val(controller__send_to_ctrl_ring_pkt__val),
		.send_to_dma_spm_rd_resp__msg(controller__send_to_dma_spm_rd_resp__msg),
		.send_to_dma_spm_rd_resp__rdy(controller__send_to_dma_spm_rd_resp__rdy),
		.send_to_dma_spm_rd_resp__val(controller__send_to_dma_spm_rd_resp__val),
		.send_to_im2col_engine_pkt__msg(controller__send_to_im2col_engine_pkt__msg),
		.send_to_im2col_engine_pkt__rdy(controller__send_to_im2col_engine_pkt__rdy),
		.send_to_im2col_engine_pkt__val(controller__send_to_im2col_engine_pkt__val),
		.send_to_inter_cgra_noc__msg(controller__send_to_inter_cgra_noc__msg),
		.send_to_inter_cgra_noc__rdy(controller__send_to_inter_cgra_noc__rdy),
		.send_to_inter_cgra_noc__val(controller__send_to_inter_cgra_noc__val),
		.send_to_sram_load_request_from_dma__msg(controller__send_to_sram_load_request_from_dma__msg),
		.send_to_sram_load_request_from_dma__rdy(controller__send_to_sram_load_request_from_dma__rdy),
		.send_to_sram_load_request_from_dma__val(controller__send_to_sram_load_request_from_dma__val),
		.send_to_sram_load_request_from_noc__msg(controller__send_to_sram_load_request_from_noc__msg),
		.send_to_sram_load_request_from_noc__rdy(controller__send_to_sram_load_request_from_noc__rdy),
		.send_to_sram_load_request_from_noc__val(controller__send_to_sram_load_request_from_noc__val),
		.send_to_sram_store_request_from_dma__msg(controller__send_to_sram_store_request_from_dma__msg),
		.send_to_sram_store_request_from_dma__rdy(controller__send_to_sram_store_request_from_dma__rdy),
		.send_to_sram_store_request_from_dma__val(controller__send_to_sram_store_request_from_dma__val),
		.send_to_sram_store_request_from_noc__msg(controller__send_to_sram_store_request_from_noc__msg),
		.send_to_sram_store_request_from_noc__rdy(controller__send_to_sram_store_request_from_noc__rdy),
		.send_to_sram_store_request_from_noc__val(controller__send_to_sram_store_request_from_noc__val),
		.send_to_tile_load_response__msg(controller__send_to_tile_load_response__msg),
		.send_to_tile_load_response__rdy(controller__send_to_tile_load_response__rdy),
		.send_to_tile_load_response__val(controller__send_to_tile_load_response__val)
	);
	wire [0:0] ctrl_ring__clk;
	wire [0:0] ctrl_ring__reset;
	wire [3365:0] ctrl_ring__recv__msg;
	wire [16:0] ctrl_ring__recv__rdy;
	wire [16:0] ctrl_ring__recv__val;
	wire [3365:0] ctrl_ring__send__msg;
	wire [16:0] ctrl_ring__send__rdy;
	wire [16:0] ctrl_ring__send__val;
	RingNetworkRTL__ad9b2c0aa02e2f3a ctrl_ring(
		.clk(ctrl_ring__clk),
		.reset(ctrl_ring__reset),
		.recv__msg(ctrl_ring__recv__msg),
		.recv__rdy(ctrl_ring__recv__rdy),
		.recv__val(ctrl_ring__recv__val),
		.send__msg(ctrl_ring__send__msg),
		.send__rdy(ctrl_ring__send__rdy),
		.send__val(ctrl_ring__send__val)
	);
	wire [0:0] tile__cgra_id [0:15];
	wire [0:0] tile__clk [0:15];
	wire [0:0] tile__reset [0:15];
	wire [4:0] tile__tile_id [0:15];
	wire [34:0] tile__from_mem_rdata__msg [0:15];
	wire [0:0] tile__from_mem_rdata__rdy [0:15];
	wire [0:0] tile__from_mem_rdata__val [0:15];
	wire [139:0] tile__recv_data__msg [0:15];
	wire [3:0] tile__recv_data__rdy [0:15];
	wire [3:0] tile__recv_data__val [0:15];
	wire [197:0] tile__recv_from_controller_pkt__msg [0:15];
	wire [0:0] tile__recv_from_controller_pkt__rdy [0:15];
	wire [0:0] tile__recv_from_controller_pkt__val [0:15];
	wire [139:0] tile__send_data__msg [0:15];
	wire [3:0] tile__send_data__rdy [0:15];
	wire [3:0] tile__send_data__val [0:15];
	wire [197:0] tile__send_to_controller_pkt__msg [0:15];
	wire [0:0] tile__send_to_controller_pkt__rdy [0:15];
	wire [0:0] tile__send_to_controller_pkt__val [0:15];
	wire [14:0] tile__to_mem_raddr__msg [0:15];
	wire [0:0] tile__to_mem_raddr__rdy [0:15];
	wire [0:0] tile__to_mem_raddr__val [0:15];
	wire [14:0] tile__to_mem_waddr__msg [0:15];
	wire [0:0] tile__to_mem_waddr__rdy [0:15];
	wire [0:0] tile__to_mem_waddr__val [0:15];
	wire [34:0] tile__to_mem_wdata__msg [0:15];
	wire [0:0] tile__to_mem_wdata__rdy [0:15];
	wire [0:0] tile__to_mem_wdata__val [0:15];
	TileWithContextSwitchRTL__70d22822c0293fa6 tile__0(
		.cgra_id(tile__cgra_id[0]),
		.clk(tile__clk[0]),
		.reset(tile__reset[0]),
		.tile_id(tile__tile_id[0]),
		.from_mem_rdata__msg(tile__from_mem_rdata__msg[0]),
		.from_mem_rdata__rdy(tile__from_mem_rdata__rdy[0]),
		.from_mem_rdata__val(tile__from_mem_rdata__val[0]),
		.recv_data__msg(tile__recv_data__msg[0]),
		.recv_data__rdy(tile__recv_data__rdy[0]),
		.recv_data__val(tile__recv_data__val[0]),
		.recv_from_controller_pkt__msg(tile__recv_from_controller_pkt__msg[0]),
		.recv_from_controller_pkt__rdy(tile__recv_from_controller_pkt__rdy[0]),
		.recv_from_controller_pkt__val(tile__recv_from_controller_pkt__val[0]),
		.send_data__msg(tile__send_data__msg[0]),
		.send_data__rdy(tile__send_data__rdy[0]),
		.send_data__val(tile__send_data__val[0]),
		.send_to_controller_pkt__msg(tile__send_to_controller_pkt__msg[0]),
		.send_to_controller_pkt__rdy(tile__send_to_controller_pkt__rdy[0]),
		.send_to_controller_pkt__val(tile__send_to_controller_pkt__val[0]),
		.to_mem_raddr__msg(tile__to_mem_raddr__msg[0]),
		.to_mem_raddr__rdy(tile__to_mem_raddr__rdy[0]),
		.to_mem_raddr__val(tile__to_mem_raddr__val[0]),
		.to_mem_waddr__msg(tile__to_mem_waddr__msg[0]),
		.to_mem_waddr__rdy(tile__to_mem_waddr__rdy[0]),
		.to_mem_waddr__val(tile__to_mem_waddr__val[0]),
		.to_mem_wdata__msg(tile__to_mem_wdata__msg[0]),
		.to_mem_wdata__rdy(tile__to_mem_wdata__rdy[0]),
		.to_mem_wdata__val(tile__to_mem_wdata__val[0])
	);
	TileWithContextSwitchRTL__70d22822c0293fa6 tile__1(
		.cgra_id(tile__cgra_id[1]),
		.clk(tile__clk[1]),
		.reset(tile__reset[1]),
		.tile_id(tile__tile_id[1]),
		.from_mem_rdata__msg(tile__from_mem_rdata__msg[1]),
		.from_mem_rdata__rdy(tile__from_mem_rdata__rdy[1]),
		.from_mem_rdata__val(tile__from_mem_rdata__val[1]),
		.recv_data__msg(tile__recv_data__msg[1]),
		.recv_data__rdy(tile__recv_data__rdy[1]),
		.recv_data__val(tile__recv_data__val[1]),
		.recv_from_controller_pkt__msg(tile__recv_from_controller_pkt__msg[1]),
		.recv_from_controller_pkt__rdy(tile__recv_from_controller_pkt__rdy[1]),
		.recv_from_controller_pkt__val(tile__recv_from_controller_pkt__val[1]),
		.send_data__msg(tile__send_data__msg[1]),
		.send_data__rdy(tile__send_data__rdy[1]),
		.send_data__val(tile__send_data__val[1]),
		.send_to_controller_pkt__msg(tile__send_to_controller_pkt__msg[1]),
		.send_to_controller_pkt__rdy(tile__send_to_controller_pkt__rdy[1]),
		.send_to_controller_pkt__val(tile__send_to_controller_pkt__val[1]),
		.to_mem_raddr__msg(tile__to_mem_raddr__msg[1]),
		.to_mem_raddr__rdy(tile__to_mem_raddr__rdy[1]),
		.to_mem_raddr__val(tile__to_mem_raddr__val[1]),
		.to_mem_waddr__msg(tile__to_mem_waddr__msg[1]),
		.to_mem_waddr__rdy(tile__to_mem_waddr__rdy[1]),
		.to_mem_waddr__val(tile__to_mem_waddr__val[1]),
		.to_mem_wdata__msg(tile__to_mem_wdata__msg[1]),
		.to_mem_wdata__rdy(tile__to_mem_wdata__rdy[1]),
		.to_mem_wdata__val(tile__to_mem_wdata__val[1])
	);
	TileWithContextSwitchRTL__70d22822c0293fa6 tile__2(
		.cgra_id(tile__cgra_id[2]),
		.clk(tile__clk[2]),
		.reset(tile__reset[2]),
		.tile_id(tile__tile_id[2]),
		.from_mem_rdata__msg(tile__from_mem_rdata__msg[2]),
		.from_mem_rdata__rdy(tile__from_mem_rdata__rdy[2]),
		.from_mem_rdata__val(tile__from_mem_rdata__val[2]),
		.recv_data__msg(tile__recv_data__msg[2]),
		.recv_data__rdy(tile__recv_data__rdy[2]),
		.recv_data__val(tile__recv_data__val[2]),
		.recv_from_controller_pkt__msg(tile__recv_from_controller_pkt__msg[2]),
		.recv_from_controller_pkt__rdy(tile__recv_from_controller_pkt__rdy[2]),
		.recv_from_controller_pkt__val(tile__recv_from_controller_pkt__val[2]),
		.send_data__msg(tile__send_data__msg[2]),
		.send_data__rdy(tile__send_data__rdy[2]),
		.send_data__val(tile__send_data__val[2]),
		.send_to_controller_pkt__msg(tile__send_to_controller_pkt__msg[2]),
		.send_to_controller_pkt__rdy(tile__send_to_controller_pkt__rdy[2]),
		.send_to_controller_pkt__val(tile__send_to_controller_pkt__val[2]),
		.to_mem_raddr__msg(tile__to_mem_raddr__msg[2]),
		.to_mem_raddr__rdy(tile__to_mem_raddr__rdy[2]),
		.to_mem_raddr__val(tile__to_mem_raddr__val[2]),
		.to_mem_waddr__msg(tile__to_mem_waddr__msg[2]),
		.to_mem_waddr__rdy(tile__to_mem_waddr__rdy[2]),
		.to_mem_waddr__val(tile__to_mem_waddr__val[2]),
		.to_mem_wdata__msg(tile__to_mem_wdata__msg[2]),
		.to_mem_wdata__rdy(tile__to_mem_wdata__rdy[2]),
		.to_mem_wdata__val(tile__to_mem_wdata__val[2])
	);
	TileWithContextSwitchRTL__70d22822c0293fa6 tile__3(
		.cgra_id(tile__cgra_id[3]),
		.clk(tile__clk[3]),
		.reset(tile__reset[3]),
		.tile_id(tile__tile_id[3]),
		.from_mem_rdata__msg(tile__from_mem_rdata__msg[3]),
		.from_mem_rdata__rdy(tile__from_mem_rdata__rdy[3]),
		.from_mem_rdata__val(tile__from_mem_rdata__val[3]),
		.recv_data__msg(tile__recv_data__msg[3]),
		.recv_data__rdy(tile__recv_data__rdy[3]),
		.recv_data__val(tile__recv_data__val[3]),
		.recv_from_controller_pkt__msg(tile__recv_from_controller_pkt__msg[3]),
		.recv_from_controller_pkt__rdy(tile__recv_from_controller_pkt__rdy[3]),
		.recv_from_controller_pkt__val(tile__recv_from_controller_pkt__val[3]),
		.send_data__msg(tile__send_data__msg[3]),
		.send_data__rdy(tile__send_data__rdy[3]),
		.send_data__val(tile__send_data__val[3]),
		.send_to_controller_pkt__msg(tile__send_to_controller_pkt__msg[3]),
		.send_to_controller_pkt__rdy(tile__send_to_controller_pkt__rdy[3]),
		.send_to_controller_pkt__val(tile__send_to_controller_pkt__val[3]),
		.to_mem_raddr__msg(tile__to_mem_raddr__msg[3]),
		.to_mem_raddr__rdy(tile__to_mem_raddr__rdy[3]),
		.to_mem_raddr__val(tile__to_mem_raddr__val[3]),
		.to_mem_waddr__msg(tile__to_mem_waddr__msg[3]),
		.to_mem_waddr__rdy(tile__to_mem_waddr__rdy[3]),
		.to_mem_waddr__val(tile__to_mem_waddr__val[3]),
		.to_mem_wdata__msg(tile__to_mem_wdata__msg[3]),
		.to_mem_wdata__rdy(tile__to_mem_wdata__rdy[3]),
		.to_mem_wdata__val(tile__to_mem_wdata__val[3])
	);
	TileWithContextSwitchRTL__70d22822c0293fa6 tile__4(
		.cgra_id(tile__cgra_id[4]),
		.clk(tile__clk[4]),
		.reset(tile__reset[4]),
		.tile_id(tile__tile_id[4]),
		.from_mem_rdata__msg(tile__from_mem_rdata__msg[4]),
		.from_mem_rdata__rdy(tile__from_mem_rdata__rdy[4]),
		.from_mem_rdata__val(tile__from_mem_rdata__val[4]),
		.recv_data__msg(tile__recv_data__msg[4]),
		.recv_data__rdy(tile__recv_data__rdy[4]),
		.recv_data__val(tile__recv_data__val[4]),
		.recv_from_controller_pkt__msg(tile__recv_from_controller_pkt__msg[4]),
		.recv_from_controller_pkt__rdy(tile__recv_from_controller_pkt__rdy[4]),
		.recv_from_controller_pkt__val(tile__recv_from_controller_pkt__val[4]),
		.send_data__msg(tile__send_data__msg[4]),
		.send_data__rdy(tile__send_data__rdy[4]),
		.send_data__val(tile__send_data__val[4]),
		.send_to_controller_pkt__msg(tile__send_to_controller_pkt__msg[4]),
		.send_to_controller_pkt__rdy(tile__send_to_controller_pkt__rdy[4]),
		.send_to_controller_pkt__val(tile__send_to_controller_pkt__val[4]),
		.to_mem_raddr__msg(tile__to_mem_raddr__msg[4]),
		.to_mem_raddr__rdy(tile__to_mem_raddr__rdy[4]),
		.to_mem_raddr__val(tile__to_mem_raddr__val[4]),
		.to_mem_waddr__msg(tile__to_mem_waddr__msg[4]),
		.to_mem_waddr__rdy(tile__to_mem_waddr__rdy[4]),
		.to_mem_waddr__val(tile__to_mem_waddr__val[4]),
		.to_mem_wdata__msg(tile__to_mem_wdata__msg[4]),
		.to_mem_wdata__rdy(tile__to_mem_wdata__rdy[4]),
		.to_mem_wdata__val(tile__to_mem_wdata__val[4])
	);
	TileWithContextSwitchRTL__70d22822c0293fa6 tile__5(
		.cgra_id(tile__cgra_id[5]),
		.clk(tile__clk[5]),
		.reset(tile__reset[5]),
		.tile_id(tile__tile_id[5]),
		.from_mem_rdata__msg(tile__from_mem_rdata__msg[5]),
		.from_mem_rdata__rdy(tile__from_mem_rdata__rdy[5]),
		.from_mem_rdata__val(tile__from_mem_rdata__val[5]),
		.recv_data__msg(tile__recv_data__msg[5]),
		.recv_data__rdy(tile__recv_data__rdy[5]),
		.recv_data__val(tile__recv_data__val[5]),
		.recv_from_controller_pkt__msg(tile__recv_from_controller_pkt__msg[5]),
		.recv_from_controller_pkt__rdy(tile__recv_from_controller_pkt__rdy[5]),
		.recv_from_controller_pkt__val(tile__recv_from_controller_pkt__val[5]),
		.send_data__msg(tile__send_data__msg[5]),
		.send_data__rdy(tile__send_data__rdy[5]),
		.send_data__val(tile__send_data__val[5]),
		.send_to_controller_pkt__msg(tile__send_to_controller_pkt__msg[5]),
		.send_to_controller_pkt__rdy(tile__send_to_controller_pkt__rdy[5]),
		.send_to_controller_pkt__val(tile__send_to_controller_pkt__val[5]),
		.to_mem_raddr__msg(tile__to_mem_raddr__msg[5]),
		.to_mem_raddr__rdy(tile__to_mem_raddr__rdy[5]),
		.to_mem_raddr__val(tile__to_mem_raddr__val[5]),
		.to_mem_waddr__msg(tile__to_mem_waddr__msg[5]),
		.to_mem_waddr__rdy(tile__to_mem_waddr__rdy[5]),
		.to_mem_waddr__val(tile__to_mem_waddr__val[5]),
		.to_mem_wdata__msg(tile__to_mem_wdata__msg[5]),
		.to_mem_wdata__rdy(tile__to_mem_wdata__rdy[5]),
		.to_mem_wdata__val(tile__to_mem_wdata__val[5])
	);
	TileWithContextSwitchRTL__70d22822c0293fa6 tile__6(
		.cgra_id(tile__cgra_id[6]),
		.clk(tile__clk[6]),
		.reset(tile__reset[6]),
		.tile_id(tile__tile_id[6]),
		.from_mem_rdata__msg(tile__from_mem_rdata__msg[6]),
		.from_mem_rdata__rdy(tile__from_mem_rdata__rdy[6]),
		.from_mem_rdata__val(tile__from_mem_rdata__val[6]),
		.recv_data__msg(tile__recv_data__msg[6]),
		.recv_data__rdy(tile__recv_data__rdy[6]),
		.recv_data__val(tile__recv_data__val[6]),
		.recv_from_controller_pkt__msg(tile__recv_from_controller_pkt__msg[6]),
		.recv_from_controller_pkt__rdy(tile__recv_from_controller_pkt__rdy[6]),
		.recv_from_controller_pkt__val(tile__recv_from_controller_pkt__val[6]),
		.send_data__msg(tile__send_data__msg[6]),
		.send_data__rdy(tile__send_data__rdy[6]),
		.send_data__val(tile__send_data__val[6]),
		.send_to_controller_pkt__msg(tile__send_to_controller_pkt__msg[6]),
		.send_to_controller_pkt__rdy(tile__send_to_controller_pkt__rdy[6]),
		.send_to_controller_pkt__val(tile__send_to_controller_pkt__val[6]),
		.to_mem_raddr__msg(tile__to_mem_raddr__msg[6]),
		.to_mem_raddr__rdy(tile__to_mem_raddr__rdy[6]),
		.to_mem_raddr__val(tile__to_mem_raddr__val[6]),
		.to_mem_waddr__msg(tile__to_mem_waddr__msg[6]),
		.to_mem_waddr__rdy(tile__to_mem_waddr__rdy[6]),
		.to_mem_waddr__val(tile__to_mem_waddr__val[6]),
		.to_mem_wdata__msg(tile__to_mem_wdata__msg[6]),
		.to_mem_wdata__rdy(tile__to_mem_wdata__rdy[6]),
		.to_mem_wdata__val(tile__to_mem_wdata__val[6])
	);
	TileWithContextSwitchRTL__70d22822c0293fa6 tile__7(
		.cgra_id(tile__cgra_id[7]),
		.clk(tile__clk[7]),
		.reset(tile__reset[7]),
		.tile_id(tile__tile_id[7]),
		.from_mem_rdata__msg(tile__from_mem_rdata__msg[7]),
		.from_mem_rdata__rdy(tile__from_mem_rdata__rdy[7]),
		.from_mem_rdata__val(tile__from_mem_rdata__val[7]),
		.recv_data__msg(tile__recv_data__msg[7]),
		.recv_data__rdy(tile__recv_data__rdy[7]),
		.recv_data__val(tile__recv_data__val[7]),
		.recv_from_controller_pkt__msg(tile__recv_from_controller_pkt__msg[7]),
		.recv_from_controller_pkt__rdy(tile__recv_from_controller_pkt__rdy[7]),
		.recv_from_controller_pkt__val(tile__recv_from_controller_pkt__val[7]),
		.send_data__msg(tile__send_data__msg[7]),
		.send_data__rdy(tile__send_data__rdy[7]),
		.send_data__val(tile__send_data__val[7]),
		.send_to_controller_pkt__msg(tile__send_to_controller_pkt__msg[7]),
		.send_to_controller_pkt__rdy(tile__send_to_controller_pkt__rdy[7]),
		.send_to_controller_pkt__val(tile__send_to_controller_pkt__val[7]),
		.to_mem_raddr__msg(tile__to_mem_raddr__msg[7]),
		.to_mem_raddr__rdy(tile__to_mem_raddr__rdy[7]),
		.to_mem_raddr__val(tile__to_mem_raddr__val[7]),
		.to_mem_waddr__msg(tile__to_mem_waddr__msg[7]),
		.to_mem_waddr__rdy(tile__to_mem_waddr__rdy[7]),
		.to_mem_waddr__val(tile__to_mem_waddr__val[7]),
		.to_mem_wdata__msg(tile__to_mem_wdata__msg[7]),
		.to_mem_wdata__rdy(tile__to_mem_wdata__rdy[7]),
		.to_mem_wdata__val(tile__to_mem_wdata__val[7])
	);
	TileWithContextSwitchRTL__70d22822c0293fa6 tile__8(
		.cgra_id(tile__cgra_id[8]),
		.clk(tile__clk[8]),
		.reset(tile__reset[8]),
		.tile_id(tile__tile_id[8]),
		.from_mem_rdata__msg(tile__from_mem_rdata__msg[8]),
		.from_mem_rdata__rdy(tile__from_mem_rdata__rdy[8]),
		.from_mem_rdata__val(tile__from_mem_rdata__val[8]),
		.recv_data__msg(tile__recv_data__msg[8]),
		.recv_data__rdy(tile__recv_data__rdy[8]),
		.recv_data__val(tile__recv_data__val[8]),
		.recv_from_controller_pkt__msg(tile__recv_from_controller_pkt__msg[8]),
		.recv_from_controller_pkt__rdy(tile__recv_from_controller_pkt__rdy[8]),
		.recv_from_controller_pkt__val(tile__recv_from_controller_pkt__val[8]),
		.send_data__msg(tile__send_data__msg[8]),
		.send_data__rdy(tile__send_data__rdy[8]),
		.send_data__val(tile__send_data__val[8]),
		.send_to_controller_pkt__msg(tile__send_to_controller_pkt__msg[8]),
		.send_to_controller_pkt__rdy(tile__send_to_controller_pkt__rdy[8]),
		.send_to_controller_pkt__val(tile__send_to_controller_pkt__val[8]),
		.to_mem_raddr__msg(tile__to_mem_raddr__msg[8]),
		.to_mem_raddr__rdy(tile__to_mem_raddr__rdy[8]),
		.to_mem_raddr__val(tile__to_mem_raddr__val[8]),
		.to_mem_waddr__msg(tile__to_mem_waddr__msg[8]),
		.to_mem_waddr__rdy(tile__to_mem_waddr__rdy[8]),
		.to_mem_waddr__val(tile__to_mem_waddr__val[8]),
		.to_mem_wdata__msg(tile__to_mem_wdata__msg[8]),
		.to_mem_wdata__rdy(tile__to_mem_wdata__rdy[8]),
		.to_mem_wdata__val(tile__to_mem_wdata__val[8])
	);
	TileWithContextSwitchRTL__70d22822c0293fa6 tile__9(
		.cgra_id(tile__cgra_id[9]),
		.clk(tile__clk[9]),
		.reset(tile__reset[9]),
		.tile_id(tile__tile_id[9]),
		.from_mem_rdata__msg(tile__from_mem_rdata__msg[9]),
		.from_mem_rdata__rdy(tile__from_mem_rdata__rdy[9]),
		.from_mem_rdata__val(tile__from_mem_rdata__val[9]),
		.recv_data__msg(tile__recv_data__msg[9]),
		.recv_data__rdy(tile__recv_data__rdy[9]),
		.recv_data__val(tile__recv_data__val[9]),
		.recv_from_controller_pkt__msg(tile__recv_from_controller_pkt__msg[9]),
		.recv_from_controller_pkt__rdy(tile__recv_from_controller_pkt__rdy[9]),
		.recv_from_controller_pkt__val(tile__recv_from_controller_pkt__val[9]),
		.send_data__msg(tile__send_data__msg[9]),
		.send_data__rdy(tile__send_data__rdy[9]),
		.send_data__val(tile__send_data__val[9]),
		.send_to_controller_pkt__msg(tile__send_to_controller_pkt__msg[9]),
		.send_to_controller_pkt__rdy(tile__send_to_controller_pkt__rdy[9]),
		.send_to_controller_pkt__val(tile__send_to_controller_pkt__val[9]),
		.to_mem_raddr__msg(tile__to_mem_raddr__msg[9]),
		.to_mem_raddr__rdy(tile__to_mem_raddr__rdy[9]),
		.to_mem_raddr__val(tile__to_mem_raddr__val[9]),
		.to_mem_waddr__msg(tile__to_mem_waddr__msg[9]),
		.to_mem_waddr__rdy(tile__to_mem_waddr__rdy[9]),
		.to_mem_waddr__val(tile__to_mem_waddr__val[9]),
		.to_mem_wdata__msg(tile__to_mem_wdata__msg[9]),
		.to_mem_wdata__rdy(tile__to_mem_wdata__rdy[9]),
		.to_mem_wdata__val(tile__to_mem_wdata__val[9])
	);
	TileWithContextSwitchRTL__70d22822c0293fa6 tile__10(
		.cgra_id(tile__cgra_id[10]),
		.clk(tile__clk[10]),
		.reset(tile__reset[10]),
		.tile_id(tile__tile_id[10]),
		.from_mem_rdata__msg(tile__from_mem_rdata__msg[10]),
		.from_mem_rdata__rdy(tile__from_mem_rdata__rdy[10]),
		.from_mem_rdata__val(tile__from_mem_rdata__val[10]),
		.recv_data__msg(tile__recv_data__msg[10]),
		.recv_data__rdy(tile__recv_data__rdy[10]),
		.recv_data__val(tile__recv_data__val[10]),
		.recv_from_controller_pkt__msg(tile__recv_from_controller_pkt__msg[10]),
		.recv_from_controller_pkt__rdy(tile__recv_from_controller_pkt__rdy[10]),
		.recv_from_controller_pkt__val(tile__recv_from_controller_pkt__val[10]),
		.send_data__msg(tile__send_data__msg[10]),
		.send_data__rdy(tile__send_data__rdy[10]),
		.send_data__val(tile__send_data__val[10]),
		.send_to_controller_pkt__msg(tile__send_to_controller_pkt__msg[10]),
		.send_to_controller_pkt__rdy(tile__send_to_controller_pkt__rdy[10]),
		.send_to_controller_pkt__val(tile__send_to_controller_pkt__val[10]),
		.to_mem_raddr__msg(tile__to_mem_raddr__msg[10]),
		.to_mem_raddr__rdy(tile__to_mem_raddr__rdy[10]),
		.to_mem_raddr__val(tile__to_mem_raddr__val[10]),
		.to_mem_waddr__msg(tile__to_mem_waddr__msg[10]),
		.to_mem_waddr__rdy(tile__to_mem_waddr__rdy[10]),
		.to_mem_waddr__val(tile__to_mem_waddr__val[10]),
		.to_mem_wdata__msg(tile__to_mem_wdata__msg[10]),
		.to_mem_wdata__rdy(tile__to_mem_wdata__rdy[10]),
		.to_mem_wdata__val(tile__to_mem_wdata__val[10])
	);
	TileWithContextSwitchRTL__70d22822c0293fa6 tile__11(
		.cgra_id(tile__cgra_id[11]),
		.clk(tile__clk[11]),
		.reset(tile__reset[11]),
		.tile_id(tile__tile_id[11]),
		.from_mem_rdata__msg(tile__from_mem_rdata__msg[11]),
		.from_mem_rdata__rdy(tile__from_mem_rdata__rdy[11]),
		.from_mem_rdata__val(tile__from_mem_rdata__val[11]),
		.recv_data__msg(tile__recv_data__msg[11]),
		.recv_data__rdy(tile__recv_data__rdy[11]),
		.recv_data__val(tile__recv_data__val[11]),
		.recv_from_controller_pkt__msg(tile__recv_from_controller_pkt__msg[11]),
		.recv_from_controller_pkt__rdy(tile__recv_from_controller_pkt__rdy[11]),
		.recv_from_controller_pkt__val(tile__recv_from_controller_pkt__val[11]),
		.send_data__msg(tile__send_data__msg[11]),
		.send_data__rdy(tile__send_data__rdy[11]),
		.send_data__val(tile__send_data__val[11]),
		.send_to_controller_pkt__msg(tile__send_to_controller_pkt__msg[11]),
		.send_to_controller_pkt__rdy(tile__send_to_controller_pkt__rdy[11]),
		.send_to_controller_pkt__val(tile__send_to_controller_pkt__val[11]),
		.to_mem_raddr__msg(tile__to_mem_raddr__msg[11]),
		.to_mem_raddr__rdy(tile__to_mem_raddr__rdy[11]),
		.to_mem_raddr__val(tile__to_mem_raddr__val[11]),
		.to_mem_waddr__msg(tile__to_mem_waddr__msg[11]),
		.to_mem_waddr__rdy(tile__to_mem_waddr__rdy[11]),
		.to_mem_waddr__val(tile__to_mem_waddr__val[11]),
		.to_mem_wdata__msg(tile__to_mem_wdata__msg[11]),
		.to_mem_wdata__rdy(tile__to_mem_wdata__rdy[11]),
		.to_mem_wdata__val(tile__to_mem_wdata__val[11])
	);
	TileWithContextSwitchRTL__70d22822c0293fa6 tile__12(
		.cgra_id(tile__cgra_id[12]),
		.clk(tile__clk[12]),
		.reset(tile__reset[12]),
		.tile_id(tile__tile_id[12]),
		.from_mem_rdata__msg(tile__from_mem_rdata__msg[12]),
		.from_mem_rdata__rdy(tile__from_mem_rdata__rdy[12]),
		.from_mem_rdata__val(tile__from_mem_rdata__val[12]),
		.recv_data__msg(tile__recv_data__msg[12]),
		.recv_data__rdy(tile__recv_data__rdy[12]),
		.recv_data__val(tile__recv_data__val[12]),
		.recv_from_controller_pkt__msg(tile__recv_from_controller_pkt__msg[12]),
		.recv_from_controller_pkt__rdy(tile__recv_from_controller_pkt__rdy[12]),
		.recv_from_controller_pkt__val(tile__recv_from_controller_pkt__val[12]),
		.send_data__msg(tile__send_data__msg[12]),
		.send_data__rdy(tile__send_data__rdy[12]),
		.send_data__val(tile__send_data__val[12]),
		.send_to_controller_pkt__msg(tile__send_to_controller_pkt__msg[12]),
		.send_to_controller_pkt__rdy(tile__send_to_controller_pkt__rdy[12]),
		.send_to_controller_pkt__val(tile__send_to_controller_pkt__val[12]),
		.to_mem_raddr__msg(tile__to_mem_raddr__msg[12]),
		.to_mem_raddr__rdy(tile__to_mem_raddr__rdy[12]),
		.to_mem_raddr__val(tile__to_mem_raddr__val[12]),
		.to_mem_waddr__msg(tile__to_mem_waddr__msg[12]),
		.to_mem_waddr__rdy(tile__to_mem_waddr__rdy[12]),
		.to_mem_waddr__val(tile__to_mem_waddr__val[12]),
		.to_mem_wdata__msg(tile__to_mem_wdata__msg[12]),
		.to_mem_wdata__rdy(tile__to_mem_wdata__rdy[12]),
		.to_mem_wdata__val(tile__to_mem_wdata__val[12])
	);
	TileWithContextSwitchRTL__70d22822c0293fa6 tile__13(
		.cgra_id(tile__cgra_id[13]),
		.clk(tile__clk[13]),
		.reset(tile__reset[13]),
		.tile_id(tile__tile_id[13]),
		.from_mem_rdata__msg(tile__from_mem_rdata__msg[13]),
		.from_mem_rdata__rdy(tile__from_mem_rdata__rdy[13]),
		.from_mem_rdata__val(tile__from_mem_rdata__val[13]),
		.recv_data__msg(tile__recv_data__msg[13]),
		.recv_data__rdy(tile__recv_data__rdy[13]),
		.recv_data__val(tile__recv_data__val[13]),
		.recv_from_controller_pkt__msg(tile__recv_from_controller_pkt__msg[13]),
		.recv_from_controller_pkt__rdy(tile__recv_from_controller_pkt__rdy[13]),
		.recv_from_controller_pkt__val(tile__recv_from_controller_pkt__val[13]),
		.send_data__msg(tile__send_data__msg[13]),
		.send_data__rdy(tile__send_data__rdy[13]),
		.send_data__val(tile__send_data__val[13]),
		.send_to_controller_pkt__msg(tile__send_to_controller_pkt__msg[13]),
		.send_to_controller_pkt__rdy(tile__send_to_controller_pkt__rdy[13]),
		.send_to_controller_pkt__val(tile__send_to_controller_pkt__val[13]),
		.to_mem_raddr__msg(tile__to_mem_raddr__msg[13]),
		.to_mem_raddr__rdy(tile__to_mem_raddr__rdy[13]),
		.to_mem_raddr__val(tile__to_mem_raddr__val[13]),
		.to_mem_waddr__msg(tile__to_mem_waddr__msg[13]),
		.to_mem_waddr__rdy(tile__to_mem_waddr__rdy[13]),
		.to_mem_waddr__val(tile__to_mem_waddr__val[13]),
		.to_mem_wdata__msg(tile__to_mem_wdata__msg[13]),
		.to_mem_wdata__rdy(tile__to_mem_wdata__rdy[13]),
		.to_mem_wdata__val(tile__to_mem_wdata__val[13])
	);
	TileWithContextSwitchRTL__70d22822c0293fa6 tile__14(
		.cgra_id(tile__cgra_id[14]),
		.clk(tile__clk[14]),
		.reset(tile__reset[14]),
		.tile_id(tile__tile_id[14]),
		.from_mem_rdata__msg(tile__from_mem_rdata__msg[14]),
		.from_mem_rdata__rdy(tile__from_mem_rdata__rdy[14]),
		.from_mem_rdata__val(tile__from_mem_rdata__val[14]),
		.recv_data__msg(tile__recv_data__msg[14]),
		.recv_data__rdy(tile__recv_data__rdy[14]),
		.recv_data__val(tile__recv_data__val[14]),
		.recv_from_controller_pkt__msg(tile__recv_from_controller_pkt__msg[14]),
		.recv_from_controller_pkt__rdy(tile__recv_from_controller_pkt__rdy[14]),
		.recv_from_controller_pkt__val(tile__recv_from_controller_pkt__val[14]),
		.send_data__msg(tile__send_data__msg[14]),
		.send_data__rdy(tile__send_data__rdy[14]),
		.send_data__val(tile__send_data__val[14]),
		.send_to_controller_pkt__msg(tile__send_to_controller_pkt__msg[14]),
		.send_to_controller_pkt__rdy(tile__send_to_controller_pkt__rdy[14]),
		.send_to_controller_pkt__val(tile__send_to_controller_pkt__val[14]),
		.to_mem_raddr__msg(tile__to_mem_raddr__msg[14]),
		.to_mem_raddr__rdy(tile__to_mem_raddr__rdy[14]),
		.to_mem_raddr__val(tile__to_mem_raddr__val[14]),
		.to_mem_waddr__msg(tile__to_mem_waddr__msg[14]),
		.to_mem_waddr__rdy(tile__to_mem_waddr__rdy[14]),
		.to_mem_waddr__val(tile__to_mem_waddr__val[14]),
		.to_mem_wdata__msg(tile__to_mem_wdata__msg[14]),
		.to_mem_wdata__rdy(tile__to_mem_wdata__rdy[14]),
		.to_mem_wdata__val(tile__to_mem_wdata__val[14])
	);
	TileWithContextSwitchRTL__70d22822c0293fa6 tile__15(
		.cgra_id(tile__cgra_id[15]),
		.clk(tile__clk[15]),
		.reset(tile__reset[15]),
		.tile_id(tile__tile_id[15]),
		.from_mem_rdata__msg(tile__from_mem_rdata__msg[15]),
		.from_mem_rdata__rdy(tile__from_mem_rdata__rdy[15]),
		.from_mem_rdata__val(tile__from_mem_rdata__val[15]),
		.recv_data__msg(tile__recv_data__msg[15]),
		.recv_data__rdy(tile__recv_data__rdy[15]),
		.recv_data__val(tile__recv_data__val[15]),
		.recv_from_controller_pkt__msg(tile__recv_from_controller_pkt__msg[15]),
		.recv_from_controller_pkt__rdy(tile__recv_from_controller_pkt__rdy[15]),
		.recv_from_controller_pkt__val(tile__recv_from_controller_pkt__val[15]),
		.send_data__msg(tile__send_data__msg[15]),
		.send_data__rdy(tile__send_data__rdy[15]),
		.send_data__val(tile__send_data__val[15]),
		.send_to_controller_pkt__msg(tile__send_to_controller_pkt__msg[15]),
		.send_to_controller_pkt__rdy(tile__send_to_controller_pkt__rdy[15]),
		.send_to_controller_pkt__val(tile__send_to_controller_pkt__val[15]),
		.to_mem_raddr__msg(tile__to_mem_raddr__msg[15]),
		.to_mem_raddr__rdy(tile__to_mem_raddr__rdy[15]),
		.to_mem_raddr__val(tile__to_mem_raddr__val[15]),
		.to_mem_waddr__msg(tile__to_mem_waddr__msg[15]),
		.to_mem_waddr__rdy(tile__to_mem_waddr__rdy[15]),
		.to_mem_waddr__val(tile__to_mem_waddr__val[15]),
		.to_mem_wdata__msg(tile__to_mem_wdata__msg[15]),
		.to_mem_wdata__rdy(tile__to_mem_wdata__rdy[15]),
		.to_mem_wdata__val(tile__to_mem_wdata__val[15])
	);
	assign tile__clk[0] = clk;
	assign tile__reset[0] = reset;
	assign tile__clk[1] = clk;
	assign tile__reset[1] = reset;
	assign tile__clk[2] = clk;
	assign tile__reset[2] = reset;
	assign tile__clk[3] = clk;
	assign tile__reset[3] = reset;
	assign tile__clk[4] = clk;
	assign tile__reset[4] = reset;
	assign tile__clk[5] = clk;
	assign tile__reset[5] = reset;
	assign tile__clk[6] = clk;
	assign tile__reset[6] = reset;
	assign tile__clk[7] = clk;
	assign tile__reset[7] = reset;
	assign tile__clk[8] = clk;
	assign tile__reset[8] = reset;
	assign tile__clk[9] = clk;
	assign tile__reset[9] = reset;
	assign tile__clk[10] = clk;
	assign tile__reset[10] = reset;
	assign tile__clk[11] = clk;
	assign tile__reset[11] = reset;
	assign tile__clk[12] = clk;
	assign tile__reset[12] = reset;
	assign tile__clk[13] = clk;
	assign tile__reset[13] = reset;
	assign tile__clk[14] = clk;
	assign tile__reset[14] = reset;
	assign tile__clk[15] = clk;
	assign tile__reset[15] = reset;
	assign data_mem__clk = clk;
	assign data_mem__reset = reset;
	assign controller__clk = clk;
	assign controller__reset = reset;
	assign controller__cgra_id = cgra_id;
	assign controller__send_to_im2col_engine_pkt__rdy = 1'd0;
	assign controller__recv_from_im2col_pkt__val = 1'd0;
	assign controller__recv_from_im2col_pkt__msg = 198'h00000000000000000000000000000000000000000000000000;
	assign ctrl_ring__clk = clk;
	assign ctrl_ring__reset = reset;
	assign data_mem__cgra_id = cgra_id;
	assign data_mem__address_lower = address_lower;
	assign data_mem__address_upper = address_upper;
	assign dma_cmd__msg = controller__dma_cmd__msg;
	assign controller__dma_cmd__rdy = dma_cmd__rdy;
	assign dma_cmd__val = controller__dma_cmd__val;
	assign controller__dma_done__msg = dma_done__msg;
	assign dma_done__rdy = controller__dma_done__rdy;
	assign controller__dma_done__val = dma_done__val;
	assign controller__recv_from_dma_spm_wr_req__msg = recv_from_dma_spm_wr_req__msg;
	assign recv_from_dma_spm_wr_req__rdy = controller__recv_from_dma_spm_wr_req__rdy;
	assign controller__recv_from_dma_spm_wr_req__val = recv_from_dma_spm_wr_req__val;
	assign controller__recv_from_dma_spm_rd_req__msg = recv_from_dma_spm_rd_req__msg;
	assign recv_from_dma_spm_rd_req__rdy = controller__recv_from_dma_spm_rd_req__rdy;
	assign controller__recv_from_dma_spm_rd_req__val = recv_from_dma_spm_rd_req__val;
	assign send_to_dma_spm_rd_resp__msg = controller__send_to_dma_spm_rd_resp__msg;
	assign controller__send_to_dma_spm_rd_resp__rdy = send_to_dma_spm_rd_resp__rdy;
	assign send_to_dma_spm_rd_resp__val = controller__send_to_dma_spm_rd_resp__val;
	assign data_mem__recv_from_controller_spm_wr_req__msg = controller__send_to_sram_store_request_from_dma__msg;
	assign controller__send_to_sram_store_request_from_dma__rdy = data_mem__recv_from_controller_spm_wr_req__rdy;
	assign data_mem__recv_from_controller_spm_wr_req__val = controller__send_to_sram_store_request_from_dma__val;
	assign data_mem__recv_from_controller_spm_rd_req__msg = controller__send_to_sram_load_request_from_dma__msg;
	assign controller__send_to_sram_load_request_from_dma__rdy = data_mem__recv_from_controller_spm_rd_req__rdy;
	assign data_mem__recv_from_controller_spm_rd_req__val = controller__send_to_sram_load_request_from_dma__val;
	assign controller__recv_from_sram_load_response__msg = data_mem__send_to_controller_spm_rd_resp__msg;
	assign data_mem__send_to_controller_spm_rd_resp__rdy = controller__recv_from_sram_load_response__rdy;
	assign controller__recv_from_sram_load_response__val = data_mem__send_to_controller_spm_rd_resp__val;
	assign data_mem__recv_from_noc_load_request__msg = controller__send_to_sram_load_request_from_noc__msg;
	assign controller__send_to_sram_load_request_from_noc__rdy = data_mem__recv_from_noc_load_request__rdy;
	assign data_mem__recv_from_noc_load_request__val = controller__send_to_sram_load_request_from_noc__val;
	assign data_mem__recv_from_noc_store_request__msg = controller__send_to_sram_store_request_from_noc__msg;
	assign controller__send_to_sram_store_request_from_noc__rdy = data_mem__recv_from_noc_store_request__rdy;
	assign data_mem__recv_from_noc_store_request__val = controller__send_to_sram_store_request_from_noc__val;
	assign data_mem__recv_from_noc_load_response_pkt__msg = controller__send_to_tile_load_response__msg;
	assign controller__send_to_tile_load_response__rdy = data_mem__recv_from_noc_load_response_pkt__rdy;
	assign data_mem__recv_from_noc_load_response_pkt__val = controller__send_to_tile_load_response__val;
	assign controller__recv_from_tile_load_request_pkt__msg = data_mem__send_to_noc_load_request_pkt__msg;
	assign data_mem__send_to_noc_load_request_pkt__rdy = controller__recv_from_tile_load_request_pkt__rdy;
	assign controller__recv_from_tile_load_request_pkt__val = data_mem__send_to_noc_load_request_pkt__val;
	assign controller__recv_from_tile_load_response_pkt__msg = data_mem__send_to_noc_load_response_pkt__msg;
	assign data_mem__send_to_noc_load_response_pkt__rdy = controller__recv_from_tile_load_response_pkt__rdy;
	assign controller__recv_from_tile_load_response_pkt__val = data_mem__send_to_noc_load_response_pkt__val;
	assign controller__recv_from_tile_store_request_pkt__msg = data_mem__send_to_noc_store_pkt__msg;
	assign data_mem__send_to_noc_store_pkt__rdy = controller__recv_from_tile_store_request_pkt__rdy;
	assign controller__recv_from_tile_store_request_pkt__val = data_mem__send_to_noc_store_pkt__val;
	assign bypass_queue__clk = clk;
	assign bypass_queue__reset = reset;
	assign controller__recv_from_inter_cgra_noc__msg = bypass_queue__send__msg;
	assign bypass_queue__send__rdy = controller__recv_from_inter_cgra_noc__rdy;
	assign controller__recv_from_inter_cgra_noc__val = bypass_queue__send__val;
	assign bypass_queue__recv__msg = controller__send_to_inter_cgra_noc__msg;
	assign controller__send_to_inter_cgra_noc__rdy = bypass_queue__recv__rdy;
	assign bypass_queue__recv__val = controller__send_to_inter_cgra_noc__val;
	assign controller__recv_from_cpu_pkt__msg = recv_from_cpu_pkt__msg;
	assign recv_from_cpu_pkt__rdy = controller__recv_from_cpu_pkt__rdy;
	assign controller__recv_from_cpu_pkt__val = recv_from_cpu_pkt__val;
	assign send_to_cpu_pkt__msg = controller__send_to_cpu_pkt__msg;
	assign controller__send_to_cpu_pkt__rdy = send_to_cpu_pkt__rdy;
	assign send_to_cpu_pkt__val = controller__send_to_cpu_pkt__val;
	assign tile__cgra_id[0] = cgra_id;
	assign tile__tile_id[0] = 5'd0;
	assign tile__cgra_id[1] = cgra_id;
	assign tile__tile_id[1] = 5'd1;
	assign tile__cgra_id[2] = cgra_id;
	assign tile__tile_id[2] = 5'd2;
	assign tile__cgra_id[3] = cgra_id;
	assign tile__tile_id[3] = 5'd3;
	assign tile__cgra_id[4] = cgra_id;
	assign tile__tile_id[4] = 5'd4;
	assign tile__cgra_id[5] = cgra_id;
	assign tile__tile_id[5] = 5'd5;
	assign tile__cgra_id[6] = cgra_id;
	assign tile__tile_id[6] = 5'd6;
	assign tile__cgra_id[7] = cgra_id;
	assign tile__tile_id[7] = 5'd7;
	assign tile__cgra_id[8] = cgra_id;
	assign tile__tile_id[8] = 5'd8;
	assign tile__cgra_id[9] = cgra_id;
	assign tile__tile_id[9] = 5'd9;
	assign tile__cgra_id[10] = cgra_id;
	assign tile__tile_id[10] = 5'd10;
	assign tile__cgra_id[11] = cgra_id;
	assign tile__tile_id[11] = 5'd11;
	assign tile__cgra_id[12] = cgra_id;
	assign tile__tile_id[12] = 5'd12;
	assign tile__cgra_id[13] = cgra_id;
	assign tile__tile_id[13] = 5'd13;
	assign tile__cgra_id[14] = cgra_id;
	assign tile__tile_id[14] = 5'd14;
	assign tile__cgra_id[15] = cgra_id;
	assign tile__tile_id[15] = 5'd15;
	assign tile__recv_from_controller_pkt__msg[0] = ctrl_ring__send__msg[3168+:198];
	assign ctrl_ring__send__rdy[16+:1] = tile__recv_from_controller_pkt__rdy[0];
	assign tile__recv_from_controller_pkt__val[0] = ctrl_ring__send__val[16+:1];
	assign ctrl_ring__recv__msg[3168+:198] = tile__send_to_controller_pkt__msg[0];
	assign tile__send_to_controller_pkt__rdy[0] = ctrl_ring__recv__rdy[16+:1];
	assign ctrl_ring__recv__val[16+:1] = tile__send_to_controller_pkt__val[0];
	assign tile__recv_from_controller_pkt__msg[1] = ctrl_ring__send__msg[2970+:198];
	assign ctrl_ring__send__rdy[15+:1] = tile__recv_from_controller_pkt__rdy[1];
	assign tile__recv_from_controller_pkt__val[1] = ctrl_ring__send__val[15+:1];
	assign ctrl_ring__recv__msg[2970+:198] = tile__send_to_controller_pkt__msg[1];
	assign tile__send_to_controller_pkt__rdy[1] = ctrl_ring__recv__rdy[15+:1];
	assign ctrl_ring__recv__val[15+:1] = tile__send_to_controller_pkt__val[1];
	assign tile__recv_from_controller_pkt__msg[2] = ctrl_ring__send__msg[2772+:198];
	assign ctrl_ring__send__rdy[14+:1] = tile__recv_from_controller_pkt__rdy[2];
	assign tile__recv_from_controller_pkt__val[2] = ctrl_ring__send__val[14+:1];
	assign ctrl_ring__recv__msg[2772+:198] = tile__send_to_controller_pkt__msg[2];
	assign tile__send_to_controller_pkt__rdy[2] = ctrl_ring__recv__rdy[14+:1];
	assign ctrl_ring__recv__val[14+:1] = tile__send_to_controller_pkt__val[2];
	assign tile__recv_from_controller_pkt__msg[3] = ctrl_ring__send__msg[2574+:198];
	assign ctrl_ring__send__rdy[13+:1] = tile__recv_from_controller_pkt__rdy[3];
	assign tile__recv_from_controller_pkt__val[3] = ctrl_ring__send__val[13+:1];
	assign ctrl_ring__recv__msg[2574+:198] = tile__send_to_controller_pkt__msg[3];
	assign tile__send_to_controller_pkt__rdy[3] = ctrl_ring__recv__rdy[13+:1];
	assign ctrl_ring__recv__val[13+:1] = tile__send_to_controller_pkt__val[3];
	assign tile__recv_from_controller_pkt__msg[4] = ctrl_ring__send__msg[2376+:198];
	assign ctrl_ring__send__rdy[12+:1] = tile__recv_from_controller_pkt__rdy[4];
	assign tile__recv_from_controller_pkt__val[4] = ctrl_ring__send__val[12+:1];
	assign ctrl_ring__recv__msg[2376+:198] = tile__send_to_controller_pkt__msg[4];
	assign tile__send_to_controller_pkt__rdy[4] = ctrl_ring__recv__rdy[12+:1];
	assign ctrl_ring__recv__val[12+:1] = tile__send_to_controller_pkt__val[4];
	assign tile__recv_from_controller_pkt__msg[5] = ctrl_ring__send__msg[2178+:198];
	assign ctrl_ring__send__rdy[11+:1] = tile__recv_from_controller_pkt__rdy[5];
	assign tile__recv_from_controller_pkt__val[5] = ctrl_ring__send__val[11+:1];
	assign ctrl_ring__recv__msg[2178+:198] = tile__send_to_controller_pkt__msg[5];
	assign tile__send_to_controller_pkt__rdy[5] = ctrl_ring__recv__rdy[11+:1];
	assign ctrl_ring__recv__val[11+:1] = tile__send_to_controller_pkt__val[5];
	assign tile__recv_from_controller_pkt__msg[6] = ctrl_ring__send__msg[1980+:198];
	assign ctrl_ring__send__rdy[10+:1] = tile__recv_from_controller_pkt__rdy[6];
	assign tile__recv_from_controller_pkt__val[6] = ctrl_ring__send__val[10+:1];
	assign ctrl_ring__recv__msg[1980+:198] = tile__send_to_controller_pkt__msg[6];
	assign tile__send_to_controller_pkt__rdy[6] = ctrl_ring__recv__rdy[10+:1];
	assign ctrl_ring__recv__val[10+:1] = tile__send_to_controller_pkt__val[6];
	assign tile__recv_from_controller_pkt__msg[7] = ctrl_ring__send__msg[1782+:198];
	assign ctrl_ring__send__rdy[9+:1] = tile__recv_from_controller_pkt__rdy[7];
	assign tile__recv_from_controller_pkt__val[7] = ctrl_ring__send__val[9+:1];
	assign ctrl_ring__recv__msg[1782+:198] = tile__send_to_controller_pkt__msg[7];
	assign tile__send_to_controller_pkt__rdy[7] = ctrl_ring__recv__rdy[9+:1];
	assign ctrl_ring__recv__val[9+:1] = tile__send_to_controller_pkt__val[7];
	assign tile__recv_from_controller_pkt__msg[8] = ctrl_ring__send__msg[1584+:198];
	assign ctrl_ring__send__rdy[8+:1] = tile__recv_from_controller_pkt__rdy[8];
	assign tile__recv_from_controller_pkt__val[8] = ctrl_ring__send__val[8+:1];
	assign ctrl_ring__recv__msg[1584+:198] = tile__send_to_controller_pkt__msg[8];
	assign tile__send_to_controller_pkt__rdy[8] = ctrl_ring__recv__rdy[8+:1];
	assign ctrl_ring__recv__val[8+:1] = tile__send_to_controller_pkt__val[8];
	assign tile__recv_from_controller_pkt__msg[9] = ctrl_ring__send__msg[1386+:198];
	assign ctrl_ring__send__rdy[7+:1] = tile__recv_from_controller_pkt__rdy[9];
	assign tile__recv_from_controller_pkt__val[9] = ctrl_ring__send__val[7+:1];
	assign ctrl_ring__recv__msg[1386+:198] = tile__send_to_controller_pkt__msg[9];
	assign tile__send_to_controller_pkt__rdy[9] = ctrl_ring__recv__rdy[7+:1];
	assign ctrl_ring__recv__val[7+:1] = tile__send_to_controller_pkt__val[9];
	assign tile__recv_from_controller_pkt__msg[10] = ctrl_ring__send__msg[1188+:198];
	assign ctrl_ring__send__rdy[6+:1] = tile__recv_from_controller_pkt__rdy[10];
	assign tile__recv_from_controller_pkt__val[10] = ctrl_ring__send__val[6+:1];
	assign ctrl_ring__recv__msg[1188+:198] = tile__send_to_controller_pkt__msg[10];
	assign tile__send_to_controller_pkt__rdy[10] = ctrl_ring__recv__rdy[6+:1];
	assign ctrl_ring__recv__val[6+:1] = tile__send_to_controller_pkt__val[10];
	assign tile__recv_from_controller_pkt__msg[11] = ctrl_ring__send__msg[990+:198];
	assign ctrl_ring__send__rdy[5+:1] = tile__recv_from_controller_pkt__rdy[11];
	assign tile__recv_from_controller_pkt__val[11] = ctrl_ring__send__val[5+:1];
	assign ctrl_ring__recv__msg[990+:198] = tile__send_to_controller_pkt__msg[11];
	assign tile__send_to_controller_pkt__rdy[11] = ctrl_ring__recv__rdy[5+:1];
	assign ctrl_ring__recv__val[5+:1] = tile__send_to_controller_pkt__val[11];
	assign tile__recv_from_controller_pkt__msg[12] = ctrl_ring__send__msg[792+:198];
	assign ctrl_ring__send__rdy[4+:1] = tile__recv_from_controller_pkt__rdy[12];
	assign tile__recv_from_controller_pkt__val[12] = ctrl_ring__send__val[4+:1];
	assign ctrl_ring__recv__msg[792+:198] = tile__send_to_controller_pkt__msg[12];
	assign tile__send_to_controller_pkt__rdy[12] = ctrl_ring__recv__rdy[4+:1];
	assign ctrl_ring__recv__val[4+:1] = tile__send_to_controller_pkt__val[12];
	assign tile__recv_from_controller_pkt__msg[13] = ctrl_ring__send__msg[594+:198];
	assign ctrl_ring__send__rdy[3+:1] = tile__recv_from_controller_pkt__rdy[13];
	assign tile__recv_from_controller_pkt__val[13] = ctrl_ring__send__val[3+:1];
	assign ctrl_ring__recv__msg[594+:198] = tile__send_to_controller_pkt__msg[13];
	assign tile__send_to_controller_pkt__rdy[13] = ctrl_ring__recv__rdy[3+:1];
	assign ctrl_ring__recv__val[3+:1] = tile__send_to_controller_pkt__val[13];
	assign tile__recv_from_controller_pkt__msg[14] = ctrl_ring__send__msg[396+:198];
	assign ctrl_ring__send__rdy[2+:1] = tile__recv_from_controller_pkt__rdy[14];
	assign tile__recv_from_controller_pkt__val[14] = ctrl_ring__send__val[2+:1];
	assign ctrl_ring__recv__msg[396+:198] = tile__send_to_controller_pkt__msg[14];
	assign tile__send_to_controller_pkt__rdy[14] = ctrl_ring__recv__rdy[2+:1];
	assign ctrl_ring__recv__val[2+:1] = tile__send_to_controller_pkt__val[14];
	assign tile__recv_from_controller_pkt__msg[15] = ctrl_ring__send__msg[198+:198];
	assign ctrl_ring__send__rdy[1+:1] = tile__recv_from_controller_pkt__rdy[15];
	assign tile__recv_from_controller_pkt__val[15] = ctrl_ring__send__val[1+:1];
	assign ctrl_ring__recv__msg[198+:198] = tile__send_to_controller_pkt__msg[15];
	assign tile__send_to_controller_pkt__rdy[15] = ctrl_ring__recv__rdy[1+:1];
	assign ctrl_ring__recv__val[1+:1] = tile__send_to_controller_pkt__val[15];
	assign ctrl_ring__recv__msg[0+:198] = controller__send_to_ctrl_ring_pkt__msg;
	assign controller__send_to_ctrl_ring_pkt__rdy = ctrl_ring__recv__rdy[0+:1];
	assign ctrl_ring__recv__val[0+:1] = controller__send_to_ctrl_ring_pkt__val;
	assign controller__recv_from_ctrl_ring_pkt__msg = ctrl_ring__send__msg[0+:198];
	assign ctrl_ring__send__rdy[0+:1] = controller__recv_from_ctrl_ring_pkt__rdy;
	assign controller__recv_from_ctrl_ring_pkt__val = ctrl_ring__send__val[0+:1];
	assign data_mem__recv_raddr__msg[90+:15] = tile__to_mem_raddr__msg[0];
	assign tile__to_mem_raddr__rdy[0] = data_mem__recv_raddr__rdy[6+:1];
	assign data_mem__recv_raddr__val[6+:1] = tile__to_mem_raddr__val[0];
	assign tile__from_mem_rdata__msg[0] = data_mem__send_rdata__msg[210+:35];
	assign data_mem__send_rdata__rdy[6+:1] = tile__from_mem_rdata__rdy[0];
	assign tile__from_mem_rdata__val[0] = data_mem__send_rdata__val[6+:1];
	assign tile__recv_data__val[0][1+:1] = 1'd0;
	assign tile__recv_data__msg[0][35+:35] = 35'h000000000;
	assign data_mem__recv_waddr__msg[90+:15] = tile__to_mem_waddr__msg[0];
	assign tile__to_mem_waddr__rdy[0] = data_mem__recv_waddr__rdy[6+:1];
	assign data_mem__recv_waddr__val[6+:1] = tile__to_mem_waddr__val[0];
	assign data_mem__recv_wdata__msg[210+:35] = tile__to_mem_wdata__msg[0];
	assign tile__to_mem_wdata__rdy[0] = data_mem__recv_wdata__rdy[6+:1];
	assign data_mem__recv_wdata__val[6+:1] = tile__to_mem_wdata__val[0];
	assign tile__send_data__rdy[0][1+:1] = 1'd0;
	assign data_mem__recv_raddr__msg[75+:15] = tile__to_mem_raddr__msg[4];
	assign tile__to_mem_raddr__rdy[4] = data_mem__recv_raddr__rdy[5+:1];
	assign data_mem__recv_raddr__val[5+:1] = tile__to_mem_raddr__val[4];
	assign tile__from_mem_rdata__msg[4] = data_mem__send_rdata__msg[175+:35];
	assign data_mem__send_rdata__rdy[5+:1] = tile__from_mem_rdata__rdy[4];
	assign tile__from_mem_rdata__val[4] = data_mem__send_rdata__val[5+:1];
	assign tile__recv_data__val[4][1+:1] = 1'd0;
	assign tile__recv_data__msg[4][35+:35] = 35'h000000000;
	assign data_mem__recv_waddr__msg[75+:15] = tile__to_mem_waddr__msg[4];
	assign tile__to_mem_waddr__rdy[4] = data_mem__recv_waddr__rdy[5+:1];
	assign data_mem__recv_waddr__val[5+:1] = tile__to_mem_waddr__val[4];
	assign data_mem__recv_wdata__msg[175+:35] = tile__to_mem_wdata__msg[4];
	assign tile__to_mem_wdata__rdy[4] = data_mem__recv_wdata__rdy[5+:1];
	assign data_mem__recv_wdata__val[5+:1] = tile__to_mem_wdata__val[4];
	assign tile__send_data__rdy[4][1+:1] = 1'd0;
	assign data_mem__recv_raddr__msg[60+:15] = tile__to_mem_raddr__msg[8];
	assign tile__to_mem_raddr__rdy[8] = data_mem__recv_raddr__rdy[4+:1];
	assign data_mem__recv_raddr__val[4+:1] = tile__to_mem_raddr__val[8];
	assign tile__from_mem_rdata__msg[8] = data_mem__send_rdata__msg[140+:35];
	assign data_mem__send_rdata__rdy[4+:1] = tile__from_mem_rdata__rdy[8];
	assign tile__from_mem_rdata__val[8] = data_mem__send_rdata__val[4+:1];
	assign tile__recv_data__val[8][1+:1] = 1'd0;
	assign tile__recv_data__msg[8][35+:35] = 35'h000000000;
	assign data_mem__recv_waddr__msg[60+:15] = tile__to_mem_waddr__msg[8];
	assign tile__to_mem_waddr__rdy[8] = data_mem__recv_waddr__rdy[4+:1];
	assign data_mem__recv_waddr__val[4+:1] = tile__to_mem_waddr__val[8];
	assign data_mem__recv_wdata__msg[140+:35] = tile__to_mem_wdata__msg[8];
	assign tile__to_mem_wdata__rdy[8] = data_mem__recv_wdata__rdy[4+:1];
	assign data_mem__recv_wdata__val[4+:1] = tile__to_mem_wdata__val[8];
	assign tile__send_data__rdy[8][1+:1] = 1'd0;
	assign data_mem__recv_raddr__msg[45+:15] = tile__to_mem_raddr__msg[12];
	assign tile__to_mem_raddr__rdy[12] = data_mem__recv_raddr__rdy[3+:1];
	assign data_mem__recv_raddr__val[3+:1] = tile__to_mem_raddr__val[12];
	assign tile__from_mem_rdata__msg[12] = data_mem__send_rdata__msg[105+:35];
	assign data_mem__send_rdata__rdy[3+:1] = tile__from_mem_rdata__rdy[12];
	assign tile__from_mem_rdata__val[12] = data_mem__send_rdata__val[3+:1];
	assign tile__recv_data__val[12][1+:1] = 1'd0;
	assign tile__recv_data__msg[12][35+:35] = 35'h000000000;
	assign data_mem__recv_waddr__msg[45+:15] = tile__to_mem_waddr__msg[12];
	assign tile__to_mem_waddr__rdy[12] = data_mem__recv_waddr__rdy[3+:1];
	assign data_mem__recv_waddr__val[3+:1] = tile__to_mem_waddr__val[12];
	assign data_mem__recv_wdata__msg[105+:35] = tile__to_mem_wdata__msg[12];
	assign tile__to_mem_wdata__rdy[12] = data_mem__recv_wdata__rdy[3+:1];
	assign data_mem__recv_wdata__val[3+:1] = tile__to_mem_wdata__val[12];
	assign tile__send_data__rdy[12][1+:1] = 1'd0;
	assign data_mem__recv_raddr__msg[30+:15] = tile__to_mem_raddr__msg[1];
	assign tile__to_mem_raddr__rdy[1] = data_mem__recv_raddr__rdy[2+:1];
	assign data_mem__recv_raddr__val[2+:1] = tile__to_mem_raddr__val[1];
	assign tile__from_mem_rdata__msg[1] = data_mem__send_rdata__msg[70+:35];
	assign data_mem__send_rdata__rdy[2+:1] = tile__from_mem_rdata__rdy[1];
	assign tile__from_mem_rdata__val[1] = data_mem__send_rdata__val[2+:1];
	assign tile__recv_data__val[1][2+:1] = 1'd0;
	assign tile__recv_data__msg[1][70+:35] = 35'h000000000;
	assign data_mem__recv_waddr__msg[30+:15] = tile__to_mem_waddr__msg[1];
	assign tile__to_mem_waddr__rdy[1] = data_mem__recv_waddr__rdy[2+:1];
	assign data_mem__recv_waddr__val[2+:1] = tile__to_mem_waddr__val[1];
	assign data_mem__recv_wdata__msg[70+:35] = tile__to_mem_wdata__msg[1];
	assign tile__to_mem_wdata__rdy[1] = data_mem__recv_wdata__rdy[2+:1];
	assign data_mem__recv_wdata__val[2+:1] = tile__to_mem_wdata__val[1];
	assign tile__send_data__rdy[1][2+:1] = 1'd0;
	assign data_mem__recv_raddr__msg[15+:15] = tile__to_mem_raddr__msg[2];
	assign tile__to_mem_raddr__rdy[2] = data_mem__recv_raddr__rdy[1+:1];
	assign data_mem__recv_raddr__val[1+:1] = tile__to_mem_raddr__val[2];
	assign tile__from_mem_rdata__msg[2] = data_mem__send_rdata__msg[35+:35];
	assign data_mem__send_rdata__rdy[1+:1] = tile__from_mem_rdata__rdy[2];
	assign tile__from_mem_rdata__val[2] = data_mem__send_rdata__val[1+:1];
	assign tile__recv_data__val[2][2+:1] = 1'd0;
	assign tile__recv_data__msg[2][70+:35] = 35'h000000000;
	assign data_mem__recv_waddr__msg[15+:15] = tile__to_mem_waddr__msg[2];
	assign tile__to_mem_waddr__rdy[2] = data_mem__recv_waddr__rdy[1+:1];
	assign data_mem__recv_waddr__val[1+:1] = tile__to_mem_waddr__val[2];
	assign data_mem__recv_wdata__msg[35+:35] = tile__to_mem_wdata__msg[2];
	assign tile__to_mem_wdata__rdy[2] = data_mem__recv_wdata__rdy[1+:1];
	assign data_mem__recv_wdata__val[1+:1] = tile__to_mem_wdata__val[2];
	assign tile__send_data__rdy[2][2+:1] = 1'd0;
	assign data_mem__recv_raddr__msg[0+:15] = tile__to_mem_raddr__msg[3];
	assign tile__to_mem_raddr__rdy[3] = data_mem__recv_raddr__rdy[0+:1];
	assign data_mem__recv_raddr__val[0+:1] = tile__to_mem_raddr__val[3];
	assign tile__from_mem_rdata__msg[3] = data_mem__send_rdata__msg[0+:35];
	assign data_mem__send_rdata__rdy[0+:1] = tile__from_mem_rdata__rdy[3];
	assign tile__from_mem_rdata__val[3] = data_mem__send_rdata__val[0+:1];
	assign tile__recv_data__val[3][2+:1] = 1'd0;
	assign tile__recv_data__msg[3][70+:35] = 35'h000000000;
	assign data_mem__recv_waddr__msg[0+:15] = tile__to_mem_waddr__msg[3];
	assign tile__to_mem_waddr__rdy[3] = data_mem__recv_waddr__rdy[0+:1];
	assign data_mem__recv_waddr__val[0+:1] = tile__to_mem_waddr__val[3];
	assign data_mem__recv_wdata__msg[0+:35] = tile__to_mem_wdata__msg[3];
	assign tile__to_mem_wdata__rdy[3] = data_mem__recv_wdata__rdy[0+:1];
	assign data_mem__recv_wdata__val[0+:1] = tile__to_mem_wdata__val[3];
	assign tile__send_data__rdy[3][2+:1] = 1'd0;
	assign tile__recv_data__msg[1][35+:35] = tile__send_data__msg[0][0+:35];
	assign tile__send_data__rdy[0][0+:1] = tile__recv_data__rdy[1][1+:1];
	assign tile__recv_data__val[1][1+:1] = tile__send_data__val[0][0+:1];
	assign tile__recv_data__msg[0][0+:35] = tile__send_data__msg[1][35+:35];
	assign tile__send_data__rdy[1][1+:1] = tile__recv_data__rdy[0][0+:1];
	assign tile__recv_data__val[0][0+:1] = tile__send_data__val[1][1+:1];
	assign tile__recv_data__msg[2][35+:35] = tile__send_data__msg[1][0+:35];
	assign tile__send_data__rdy[1][0+:1] = tile__recv_data__rdy[2][1+:1];
	assign tile__recv_data__val[2][1+:1] = tile__send_data__val[1][0+:1];
	assign tile__recv_data__msg[1][0+:35] = tile__send_data__msg[2][35+:35];
	assign tile__send_data__rdy[2][1+:1] = tile__recv_data__rdy[1][0+:1];
	assign tile__recv_data__val[1][0+:1] = tile__send_data__val[2][1+:1];
	assign tile__recv_data__msg[3][35+:35] = tile__send_data__msg[2][0+:35];
	assign tile__send_data__rdy[2][0+:1] = tile__recv_data__rdy[3][1+:1];
	assign tile__recv_data__val[3][1+:1] = tile__send_data__val[2][0+:1];
	assign tile__recv_data__msg[2][0+:35] = tile__send_data__msg[3][35+:35];
	assign tile__send_data__rdy[3][1+:1] = tile__recv_data__rdy[2][0+:1];
	assign tile__recv_data__val[2][0+:1] = tile__send_data__val[3][1+:1];
	assign tile__recv_data__msg[5][35+:35] = tile__send_data__msg[4][0+:35];
	assign tile__send_data__rdy[4][0+:1] = tile__recv_data__rdy[5][1+:1];
	assign tile__recv_data__val[5][1+:1] = tile__send_data__val[4][0+:1];
	assign tile__recv_data__msg[4][0+:35] = tile__send_data__msg[5][35+:35];
	assign tile__send_data__rdy[5][1+:1] = tile__recv_data__rdy[4][0+:1];
	assign tile__recv_data__val[4][0+:1] = tile__send_data__val[5][1+:1];
	assign tile__recv_data__msg[6][35+:35] = tile__send_data__msg[5][0+:35];
	assign tile__send_data__rdy[5][0+:1] = tile__recv_data__rdy[6][1+:1];
	assign tile__recv_data__val[6][1+:1] = tile__send_data__val[5][0+:1];
	assign tile__recv_data__msg[5][0+:35] = tile__send_data__msg[6][35+:35];
	assign tile__send_data__rdy[6][1+:1] = tile__recv_data__rdy[5][0+:1];
	assign tile__recv_data__val[5][0+:1] = tile__send_data__val[6][1+:1];
	assign tile__recv_data__msg[7][35+:35] = tile__send_data__msg[6][0+:35];
	assign tile__send_data__rdy[6][0+:1] = tile__recv_data__rdy[7][1+:1];
	assign tile__recv_data__val[7][1+:1] = tile__send_data__val[6][0+:1];
	assign tile__recv_data__msg[6][0+:35] = tile__send_data__msg[7][35+:35];
	assign tile__send_data__rdy[7][1+:1] = tile__recv_data__rdy[6][0+:1];
	assign tile__recv_data__val[6][0+:1] = tile__send_data__val[7][1+:1];
	assign tile__recv_data__msg[9][35+:35] = tile__send_data__msg[8][0+:35];
	assign tile__send_data__rdy[8][0+:1] = tile__recv_data__rdy[9][1+:1];
	assign tile__recv_data__val[9][1+:1] = tile__send_data__val[8][0+:1];
	assign tile__recv_data__msg[8][0+:35] = tile__send_data__msg[9][35+:35];
	assign tile__send_data__rdy[9][1+:1] = tile__recv_data__rdy[8][0+:1];
	assign tile__recv_data__val[8][0+:1] = tile__send_data__val[9][1+:1];
	assign tile__recv_data__msg[10][35+:35] = tile__send_data__msg[9][0+:35];
	assign tile__send_data__rdy[9][0+:1] = tile__recv_data__rdy[10][1+:1];
	assign tile__recv_data__val[10][1+:1] = tile__send_data__val[9][0+:1];
	assign tile__recv_data__msg[9][0+:35] = tile__send_data__msg[10][35+:35];
	assign tile__send_data__rdy[10][1+:1] = tile__recv_data__rdy[9][0+:1];
	assign tile__recv_data__val[9][0+:1] = tile__send_data__val[10][1+:1];
	assign tile__recv_data__msg[11][35+:35] = tile__send_data__msg[10][0+:35];
	assign tile__send_data__rdy[10][0+:1] = tile__recv_data__rdy[11][1+:1];
	assign tile__recv_data__val[11][1+:1] = tile__send_data__val[10][0+:1];
	assign tile__recv_data__msg[10][0+:35] = tile__send_data__msg[11][35+:35];
	assign tile__send_data__rdy[11][1+:1] = tile__recv_data__rdy[10][0+:1];
	assign tile__recv_data__val[10][0+:1] = tile__send_data__val[11][1+:1];
	assign tile__recv_data__msg[13][35+:35] = tile__send_data__msg[12][0+:35];
	assign tile__send_data__rdy[12][0+:1] = tile__recv_data__rdy[13][1+:1];
	assign tile__recv_data__val[13][1+:1] = tile__send_data__val[12][0+:1];
	assign tile__recv_data__msg[12][0+:35] = tile__send_data__msg[13][35+:35];
	assign tile__send_data__rdy[13][1+:1] = tile__recv_data__rdy[12][0+:1];
	assign tile__recv_data__val[12][0+:1] = tile__send_data__val[13][1+:1];
	assign tile__recv_data__msg[14][35+:35] = tile__send_data__msg[13][0+:35];
	assign tile__send_data__rdy[13][0+:1] = tile__recv_data__rdy[14][1+:1];
	assign tile__recv_data__val[14][1+:1] = tile__send_data__val[13][0+:1];
	assign tile__recv_data__msg[13][0+:35] = tile__send_data__msg[14][35+:35];
	assign tile__send_data__rdy[14][1+:1] = tile__recv_data__rdy[13][0+:1];
	assign tile__recv_data__val[13][0+:1] = tile__send_data__val[14][1+:1];
	assign tile__recv_data__msg[15][35+:35] = tile__send_data__msg[14][0+:35];
	assign tile__send_data__rdy[14][0+:1] = tile__recv_data__rdy[15][1+:1];
	assign tile__recv_data__val[15][1+:1] = tile__send_data__val[14][0+:1];
	assign tile__recv_data__msg[14][0+:35] = tile__send_data__msg[15][35+:35];
	assign tile__send_data__rdy[15][1+:1] = tile__recv_data__rdy[14][0+:1];
	assign tile__recv_data__val[14][0+:1] = tile__send_data__val[15][1+:1];
	assign tile__recv_data__msg[4][70+:35] = tile__send_data__msg[0][105+:35];
	assign tile__send_data__rdy[0][3+:1] = tile__recv_data__rdy[4][2+:1];
	assign tile__recv_data__val[4][2+:1] = tile__send_data__val[0][3+:1];
	assign tile__recv_data__msg[0][105+:35] = tile__send_data__msg[4][70+:35];
	assign tile__send_data__rdy[4][2+:1] = tile__recv_data__rdy[0][3+:1];
	assign tile__recv_data__val[0][3+:1] = tile__send_data__val[4][2+:1];
	assign tile__recv_data__msg[5][70+:35] = tile__send_data__msg[1][105+:35];
	assign tile__send_data__rdy[1][3+:1] = tile__recv_data__rdy[5][2+:1];
	assign tile__recv_data__val[5][2+:1] = tile__send_data__val[1][3+:1];
	assign tile__recv_data__msg[1][105+:35] = tile__send_data__msg[5][70+:35];
	assign tile__send_data__rdy[5][2+:1] = tile__recv_data__rdy[1][3+:1];
	assign tile__recv_data__val[1][3+:1] = tile__send_data__val[5][2+:1];
	assign tile__recv_data__msg[6][70+:35] = tile__send_data__msg[2][105+:35];
	assign tile__send_data__rdy[2][3+:1] = tile__recv_data__rdy[6][2+:1];
	assign tile__recv_data__val[6][2+:1] = tile__send_data__val[2][3+:1];
	assign tile__recv_data__msg[2][105+:35] = tile__send_data__msg[6][70+:35];
	assign tile__send_data__rdy[6][2+:1] = tile__recv_data__rdy[2][3+:1];
	assign tile__recv_data__val[2][3+:1] = tile__send_data__val[6][2+:1];
	assign tile__recv_data__msg[7][70+:35] = tile__send_data__msg[3][105+:35];
	assign tile__send_data__rdy[3][3+:1] = tile__recv_data__rdy[7][2+:1];
	assign tile__recv_data__val[7][2+:1] = tile__send_data__val[3][3+:1];
	assign tile__recv_data__msg[3][105+:35] = tile__send_data__msg[7][70+:35];
	assign tile__send_data__rdy[7][2+:1] = tile__recv_data__rdy[3][3+:1];
	assign tile__recv_data__val[3][3+:1] = tile__send_data__val[7][2+:1];
	assign tile__recv_data__msg[8][70+:35] = tile__send_data__msg[4][105+:35];
	assign tile__send_data__rdy[4][3+:1] = tile__recv_data__rdy[8][2+:1];
	assign tile__recv_data__val[8][2+:1] = tile__send_data__val[4][3+:1];
	assign tile__recv_data__msg[4][105+:35] = tile__send_data__msg[8][70+:35];
	assign tile__send_data__rdy[8][2+:1] = tile__recv_data__rdy[4][3+:1];
	assign tile__recv_data__val[4][3+:1] = tile__send_data__val[8][2+:1];
	assign tile__recv_data__msg[9][70+:35] = tile__send_data__msg[5][105+:35];
	assign tile__send_data__rdy[5][3+:1] = tile__recv_data__rdy[9][2+:1];
	assign tile__recv_data__val[9][2+:1] = tile__send_data__val[5][3+:1];
	assign tile__recv_data__msg[5][105+:35] = tile__send_data__msg[9][70+:35];
	assign tile__send_data__rdy[9][2+:1] = tile__recv_data__rdy[5][3+:1];
	assign tile__recv_data__val[5][3+:1] = tile__send_data__val[9][2+:1];
	assign tile__recv_data__msg[10][70+:35] = tile__send_data__msg[6][105+:35];
	assign tile__send_data__rdy[6][3+:1] = tile__recv_data__rdy[10][2+:1];
	assign tile__recv_data__val[10][2+:1] = tile__send_data__val[6][3+:1];
	assign tile__recv_data__msg[6][105+:35] = tile__send_data__msg[10][70+:35];
	assign tile__send_data__rdy[10][2+:1] = tile__recv_data__rdy[6][3+:1];
	assign tile__recv_data__val[6][3+:1] = tile__send_data__val[10][2+:1];
	assign tile__recv_data__msg[11][70+:35] = tile__send_data__msg[7][105+:35];
	assign tile__send_data__rdy[7][3+:1] = tile__recv_data__rdy[11][2+:1];
	assign tile__recv_data__val[11][2+:1] = tile__send_data__val[7][3+:1];
	assign tile__recv_data__msg[7][105+:35] = tile__send_data__msg[11][70+:35];
	assign tile__send_data__rdy[11][2+:1] = tile__recv_data__rdy[7][3+:1];
	assign tile__recv_data__val[7][3+:1] = tile__send_data__val[11][2+:1];
	assign tile__recv_data__msg[12][70+:35] = tile__send_data__msg[8][105+:35];
	assign tile__send_data__rdy[8][3+:1] = tile__recv_data__rdy[12][2+:1];
	assign tile__recv_data__val[12][2+:1] = tile__send_data__val[8][3+:1];
	assign tile__recv_data__msg[8][105+:35] = tile__send_data__msg[12][70+:35];
	assign tile__send_data__rdy[12][2+:1] = tile__recv_data__rdy[8][3+:1];
	assign tile__recv_data__val[8][3+:1] = tile__send_data__val[12][2+:1];
	assign tile__recv_data__msg[13][70+:35] = tile__send_data__msg[9][105+:35];
	assign tile__send_data__rdy[9][3+:1] = tile__recv_data__rdy[13][2+:1];
	assign tile__recv_data__val[13][2+:1] = tile__send_data__val[9][3+:1];
	assign tile__recv_data__msg[9][105+:35] = tile__send_data__msg[13][70+:35];
	assign tile__send_data__rdy[13][2+:1] = tile__recv_data__rdy[9][3+:1];
	assign tile__recv_data__val[9][3+:1] = tile__send_data__val[13][2+:1];
	assign tile__recv_data__msg[14][70+:35] = tile__send_data__msg[10][105+:35];
	assign tile__send_data__rdy[10][3+:1] = tile__recv_data__rdy[14][2+:1];
	assign tile__recv_data__val[14][2+:1] = tile__send_data__val[10][3+:1];
	assign tile__recv_data__msg[10][105+:35] = tile__send_data__msg[14][70+:35];
	assign tile__send_data__rdy[14][2+:1] = tile__recv_data__rdy[10][3+:1];
	assign tile__recv_data__val[10][3+:1] = tile__send_data__val[14][2+:1];
	assign tile__recv_data__msg[15][70+:35] = tile__send_data__msg[11][105+:35];
	assign tile__send_data__rdy[11][3+:1] = tile__recv_data__rdy[15][2+:1];
	assign tile__recv_data__val[15][2+:1] = tile__send_data__val[11][3+:1];
	assign tile__recv_data__msg[11][105+:35] = tile__send_data__msg[15][70+:35];
	assign tile__send_data__rdy[15][2+:1] = tile__recv_data__rdy[11][3+:1];
	assign tile__recv_data__val[11][3+:1] = tile__send_data__val[15][2+:1];
	assign tile__recv_data__val[0][2+:1] = 1'd0;
	assign tile__recv_data__msg[0][70+:35] = 35'h000000000;
	assign tile__send_data__rdy[0][2+:1] = 1'd0;
	assign tile__recv_data__val[3][0+:1] = 1'd0;
	assign tile__recv_data__msg[3][0+:35] = 35'h000000000;
	assign tile__send_data__rdy[3][0+:1] = 1'd0;
	assign tile__to_mem_raddr__rdy[5] = 1'd0;
	assign tile__from_mem_rdata__val[5] = 1'd0;
	assign tile__from_mem_rdata__msg[5] = 35'h000000000;
	assign tile__to_mem_waddr__rdy[5] = 1'd0;
	assign tile__to_mem_wdata__rdy[5] = 1'd0;
	assign tile__to_mem_raddr__rdy[6] = 1'd0;
	assign tile__from_mem_rdata__val[6] = 1'd0;
	assign tile__from_mem_rdata__msg[6] = 35'h000000000;
	assign tile__to_mem_waddr__rdy[6] = 1'd0;
	assign tile__to_mem_wdata__rdy[6] = 1'd0;
	assign tile__recv_data__val[7][0+:1] = 1'd0;
	assign tile__recv_data__msg[7][0+:35] = 35'h000000000;
	assign tile__send_data__rdy[7][0+:1] = 1'd0;
	assign tile__to_mem_raddr__rdy[7] = 1'd0;
	assign tile__from_mem_rdata__val[7] = 1'd0;
	assign tile__from_mem_rdata__msg[7] = 35'h000000000;
	assign tile__to_mem_waddr__rdy[7] = 1'd0;
	assign tile__to_mem_wdata__rdy[7] = 1'd0;
	assign tile__to_mem_raddr__rdy[9] = 1'd0;
	assign tile__from_mem_rdata__val[9] = 1'd0;
	assign tile__from_mem_rdata__msg[9] = 35'h000000000;
	assign tile__to_mem_waddr__rdy[9] = 1'd0;
	assign tile__to_mem_wdata__rdy[9] = 1'd0;
	assign tile__to_mem_raddr__rdy[10] = 1'd0;
	assign tile__from_mem_rdata__val[10] = 1'd0;
	assign tile__from_mem_rdata__msg[10] = 35'h000000000;
	assign tile__to_mem_waddr__rdy[10] = 1'd0;
	assign tile__to_mem_wdata__rdy[10] = 1'd0;
	assign tile__recv_data__val[11][0+:1] = 1'd0;
	assign tile__recv_data__msg[11][0+:35] = 35'h000000000;
	assign tile__send_data__rdy[11][0+:1] = 1'd0;
	assign tile__to_mem_raddr__rdy[11] = 1'd0;
	assign tile__from_mem_rdata__val[11] = 1'd0;
	assign tile__from_mem_rdata__msg[11] = 35'h000000000;
	assign tile__to_mem_waddr__rdy[11] = 1'd0;
	assign tile__to_mem_wdata__rdy[11] = 1'd0;
	assign tile__recv_data__val[12][3+:1] = 1'd0;
	assign tile__recv_data__msg[12][105+:35] = 35'h000000000;
	assign tile__send_data__rdy[12][3+:1] = 1'd0;
	assign tile__recv_data__val[13][3+:1] = 1'd0;
	assign tile__recv_data__msg[13][105+:35] = 35'h000000000;
	assign tile__send_data__rdy[13][3+:1] = 1'd0;
	assign tile__to_mem_raddr__rdy[13] = 1'd0;
	assign tile__from_mem_rdata__val[13] = 1'd0;
	assign tile__from_mem_rdata__msg[13] = 35'h000000000;
	assign tile__to_mem_waddr__rdy[13] = 1'd0;
	assign tile__to_mem_wdata__rdy[13] = 1'd0;
	assign tile__recv_data__val[14][3+:1] = 1'd0;
	assign tile__recv_data__msg[14][105+:35] = 35'h000000000;
	assign tile__send_data__rdy[14][3+:1] = 1'd0;
	assign tile__to_mem_raddr__rdy[14] = 1'd0;
	assign tile__from_mem_rdata__val[14] = 1'd0;
	assign tile__from_mem_rdata__msg[14] = 35'h000000000;
	assign tile__to_mem_waddr__rdy[14] = 1'd0;
	assign tile__to_mem_wdata__rdy[14] = 1'd0;
	assign tile__recv_data__val[15][3+:1] = 1'd0;
	assign tile__recv_data__msg[15][105+:35] = 35'h000000000;
	assign tile__recv_data__val[15][0+:1] = 1'd0;
	assign tile__recv_data__msg[15][0+:35] = 35'h000000000;
	assign tile__send_data__rdy[15][3+:1] = 1'd0;
	assign tile__send_data__rdy[15][0+:1] = 1'd0;
	assign tile__to_mem_raddr__rdy[15] = 1'd0;
	assign tile__from_mem_rdata__val[15] = 1'd0;
	assign tile__from_mem_rdata__msg[15] = 35'h000000000;
	assign tile__to_mem_waddr__rdy[15] = 1'd0;
	assign tile__to_mem_wdata__rdy[15] = 1'd0;
endmodule
