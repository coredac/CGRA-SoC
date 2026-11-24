// Two-column causal Softmax, matching attention_data.probabilities.
module GptSoftmax (
  input logic clk, reset, start, fused,
  input logic [2:0] phase,
  input logic row,
  input logic write,
  input logic [9:0] address,
  input logic signed [31:0] wdata,
  output logic signed [31:0] rdata,
  output logic busy, done
);
  logic signed [31:0] source [2], result [2], maximum;
  logic [15:0] exponent [2], exp_value;
  logic [16:0] total, square;
  logic [1:0] stage;
  logic [3:0] step;
  logic index, limit, automatic_run;
  logic signed [31:0] difference;
  logic [31:0] halved;
  logic [12:0] distance, correction, base;
  logic [20:0] scaled;
  logic [4:0] shift;
  logic [8:0] polynomial;
  logic [17:0] biased;
  logic [22:0] product, numerator;
  logic div_busy, div_done;
  logic [31:0] div_result;

  assign rdata = result[address[0]];
  assign halved = 32'(difference >>> 1);
  GptDivider divider (
    .clk(clk), .reset(reset), .start(busy && stage == 2 && step == 2),
    .signed_mode(1'b0), .dividend({9'd0, numerator}), .divisor({15'd0, total}),
    .quotient(div_result), .busy(div_busy), .done(div_done)
  );

  task automatic advance(input logic [1:0] next_stage);
    begin
      index <= 0;
      step <= 0;
      if (automatic_run && stage != 2) stage <= next_stage;
      else begin busy <= 0; done <= 1; end
    end
  endtask

  always_ff @(posedge clk) begin
    done <= 0;
    if (reset) begin
      busy <= 0; stage <= 0; step <= 0; index <= 0; limit <= 0; automatic_run <= 0;
      maximum <= 0; total <= 0;
    end else if (!busy) begin
      if (write) source[address[0]] <= wdata;
      if (start) begin
        busy <= 1; automatic_run <= fused; stage <= fused ? 2'd0 : phase[1:0];
        index <= 0; step <= 0; limit <= row;
        if (fused || phase == 0) maximum <= -32'sd2147483647 - 1;
        if (fused || phase == 1) total <= 0;
      end
    end else begin
      case (stage)
        0: begin
          if (index <= limit && source[index] > maximum) maximum <= source[index];
          index <= 1;
          if (index) advance(1);
        end
        1: begin
          step <= step + 1'b1;
          // Clamping bounds polynomial to167..346, exponent to45650 and sum to91300.
          case (step)
            0: difference <= maximum - source[index];
            1: distance <= halved > 4096 ? 13'd4096 : halved[12:0];
            2: scaled <= 21'(distance) * 21'd370;
            3: shift <= scaled[20:16];
            4: correction <= 13'(shift) * 13'd177;
            5: base <= 13'd346 + correction;
            6: polynomial <= 9'(base - distance);
            7: square <= 17'(polynomial) * 17'(polynomial);
            8: biased <= {1'b0, square} + 18'd62885;
            9: exp_value <= 16'(biased >> (shift + 5'd2));
            10: begin
              exponent[index] <= index <= limit ? exp_value : 16'd0;
              if (index <= limit) total <= total + {1'b0, exp_value};
              step <= 0;
              index <= 1;
              if (index) advance(2);
            end
            default: step <= 0;
          endcase
        end
        2: begin
          case (step)
            0: begin product <= 23'(exponent[index]) * 23'd127; step <= 1; end
            1: begin numerator <= product + {7'd0, total[16:1]}; step <= 2; end
            2: step <= 3;
            3: if (div_done) begin
              result[index] <= div_result;
              step <= 0;
              index <= 1;
              if (index) advance(0);
            end
            default: step <= 0;
          endcase
        end
        default: begin busy <= 0; done <= 1; end
      endcase
    end
  end
endmodule
