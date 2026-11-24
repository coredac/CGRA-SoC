// Fixed H128 LayerNorm with the repository's Q6/Q8 integer arithmetic.
module GptNorm (
  input logic clk, reset, start, fused,
  input logic [2:0] phase,
  input logic write,
  input logic [1:0] bank,
  input logic [9:0] address,
  input logic signed [31:0] wdata,
  output logic signed [31:0] rdata,
  output logic busy, done
);
  logic signed [31:0] source [128], gain [128], offset [128], result [128];
  logic [2:0] stage, step;
  logic automatic_run;
  logic [6:0] index;
  logic signed [31:0] sum, mean;
  logic [31:0] squares, variance, std;
  logic [31:0] radicand;
  logic [33:0] remainder;
  logic [15:0] root;
  logic [4:0] iteration;
  logic signed [31:0] sample, factor, bias, delta, product, affine, rounded;
  logic signed [63:0] square;
  logic [33:0] next_rem, trial;
  logic [15:0] next_root;
  logic div_busy, div_done;
  logic [31:0] div_result;

  GptDivider divider (
    .clk(clk), .reset(reset), .start(busy && stage == 5 && step == 3),
    .signed_mode(1'b1), .dividend(product), .divisor(std),
    .quotient(div_result), .busy(div_busy), .done(div_done)
  );

  always_comb begin
    rdata = result[address[6:0]];
    next_rem = (remainder << 2) | {32'b0, radicand[31:30]};
    trial = ({18'b0, root} << 2) | 34'd1;
    next_root = {root[14:0], (next_rem >= trial)};
  end

  task automatic advance(input logic [2:0] next_stage);
    begin
      index <= 0;
      step <= 0;
      if (automatic_run && stage != 6) stage <= next_stage;
      else begin busy <= 0; done <= 1; end
    end
  endtask

  always_ff @(posedge clk) begin
    done <= 0;
    if (reset) begin
      busy <= 0;
      stage <= 0;
      index <= 0;
      automatic_run <= 0;
      sum <= 0; mean <= 0; squares <= 0; variance <= 0; std <= 0;
      radicand <= 0; remainder <= 0; root <= 0; iteration <= 0;
      step <= 0;
    end else if (!busy) begin
      if (write) begin
        case (bank)
          0: source[address[6:0]] <= wdata;
          1: gain[address[6:0]] <= wdata;
          2: offset[address[6:0]] <= wdata;
          default: result[address[6:0]] <= wdata;
        endcase
      end
      if (start) begin
        busy <= 1;
        automatic_run <= fused;
        stage <= fused ? 3'd0 : phase;
        step <= 0;
        index <= 0;
        if (fused || phase == 0) sum <= 0;
        if (phase == 2) squares <= 0;
        if (phase == 4) begin
          radicand <= (variance << 4) + 4096;
          remainder <= 0; root <= 0; iteration <= 0;
        end
      end
    end else begin
      case (stage)
        0: begin
          if (step == 0) begin
            sample <= source[index];
            step <= 1;
          end else begin
            sum <= sum + sample;
            step <= 0;
            index <= index + 1'b1;
            if (index == 127) advance(1);
          end
        end
        1: begin
          // Width is 128, so arithmetic shift implements the required floor.
          mean <= (sum <<< 6) >>> 7;
          squares <= 0;
          advance(2);
        end
        2: begin
          case (step)
            0: begin sample <= source[index]; step <= 1; end
            1: begin delta <= (sample <<< 6) - mean; step <= 2; end
            2: begin square <= 64'(delta) * 64'(delta); step <= 3; end
            3: begin
              squares <= squares + 32'(square >> 4);
              step <= 0;
              index <= index + 1'b1;
              if (index == 127) advance(3);
            end
            default: step <= 0;
          endcase
        end
        3: begin
          variance <= squares >> 7;
          radicand <= ((squares >> 7) << 4) + 4096;
          remainder <= 0; root <= 0; iteration <= 0;
          advance(4);
        end
        4: begin
          root <= next_root;
          remainder <= next_rem >= trial ? next_rem - trial : next_rem;
          radicand <= radicand << 2;
          iteration <= iteration + 1'b1;
          if (iteration == 15) begin std <= {16'b0, next_root}; advance(5); end
        end
        5: begin
          case (step)
            0: begin
              sample <= source[index];
              factor <= gain[index];
              bias <= offset[index];
              step <= 1;
            end
            1: begin delta <= (sample <<< 6) - mean; step <= 2; end
            2: begin product <= delta * factor; step <= 3; end
            3: step <= 4;
            4: if (div_done) begin affine <= $signed(div_result) + bias; step <= 5; end
            5: begin
              result[index] <= affine;
              step <= 0;
              index <= index + 1'b1;
              if (index == 127) advance(6);
            end
            default: step <= 0;
          endcase
        end
        6: begin
          case (step)
            0: begin sample <= result[index]; step <= 1; end
            1: begin
              rounded <= sample < 0 ? -(((-sample) + 2) >>> 2) : (sample + 2) >>> 2;
              step <= 2;
            end
            2: begin
              result[index] <= rounded > 127 ? 127 : rounded < -128 ? -128 : rounded;
              step <= 0;
              index <= index + 1'b1;
              if (index == 127) advance(0);
            end
            default: step <= 0;
          endcase
        end
        default: begin busy <= 0; done <= 1; end
      endcase
    end
  end
endmodule
