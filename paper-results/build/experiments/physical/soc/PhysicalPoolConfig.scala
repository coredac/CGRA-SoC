package chipyard

import chipyard.socgen.generated.{CGRASpmWindowGenerated, GemminiExternalSpmGenerated}
import chipyard.socgen.gemmini.{WithGemminiExternalSpm, WithGemminiLink, WithGemminiRoCC}
import chipyard.socgen.cgra.WithCgraLink
import chipyard.socgen.pool.{PoolParams, WithPoolAccelerator, WithPoolLink}
import chipyard.socgen.link.WithAutoLink
import org.chipsalliance.cde.config.Config

object PhysicalPoolConfig {
  val links = CGRAMinimalGemminiPoolAutoLinkRocketConfig
  val array = CGRAMinimalGemminiRocketConfig.minimalGemminiConfig.copy(
    sp_capacity = gemmini.CapacityInKilobytes(16),
    acc_capacity = gemmini.CapacityInKilobytes(16))
}

class PhysicalPoolConfig extends Config(
  new freechips.rocketchip.subsystem.WithoutTLMonitors ++
  new testchipip.soc.WithMbusScratchpad(base = 0x08000000, size = 16 * 1024) ++
  new testchipip.soc.WithNoScratchpads ++
  new chipyard.config.WithBroadcastManager ++
  new WithCgraLink(PhysicalPoolConfig.links.cgraLink) ++
  new WithPoolLink(PhysicalPoolConfig.links.poolLink) ++
  new WithGemminiLink(PhysicalPoolConfig.links.gemminiLink) ++
  new WithAutoLink(PhysicalPoolConfig.links.autoLink) ++
  new chipyard.example.WithCGRASpmWindow(CGRASpmWindowGenerated.params) ++
  new WithPoolAccelerator(PoolParams(elementBits = 32)) ++
  new WithGemminiExternalSpm(GemminiExternalSpmGenerated.params) ++
  new chipyard.config.WithCGRA() ++
  new WithGemminiRoCC(PhysicalPoolConfig.array,
    linkParams = Some(PhysicalPoolConfig.links.gemminiLink.adapter)) ++
  new freechips.rocketchip.rocket.WithNBigCores(1) ++
  new chipyard.config.WithSystemBusWidth(256) ++
  new chipyard.config.AbstractConfig)
