# SMT 程序文件格式（formatVersion 1.0）

一个 SMT 程序是一个 UTF-8 编码的 XML 文件，扩展名 `.smt.xml`。完整示例见 [samples/example.smt.xml](../samples/example.smt.xml)。

## 1. 设计原则

1. **一个文件自包含**：程序用到的板、拼板、元件、封装、送料器都在同一个文件里，拷到另一台机器能直接打开。以后加全局元件库时，库是"导入来源"，程序里仍保存一份副本。
2. **产品数据与机台数据分开**：`Boards` / `Panel` / `Components` / `Packages` 只和产品有关，换机器不用改；`FeederSetup` 和具体机台的站位相关，换机器只需重做这一段。
3. **全部用 id 引用，不靠顺序**：元素之间通过字符串 id 关联，调整顺序、插入新元素不会破坏引用。
4. **单位写死在规范里**：长度一律 mm，角度一律 °，文件中不出现单位属性，避免单位混用。
5. **为扩展预留位置**：版本号规则（第 6 节）、送料器的类型专属参数块、`Extensions` 扩展区。

## 2. 坐标约定

- **板坐标**：原点为板左下角（从顶面看），X 向右，Y 向上。`Placement`、`Fiducial`、`BadMark` 的坐标都是板坐标。
- **底面元件**：坐标仍按"从顶面看"给出（与 CAD 导出的贴片坐标一致），由运行时在贴底面时做镜像，文件里不预先镜像。
- **拼板坐标**：原点为拼板左下角，`BoardInstance` 的 `x/y` 是该板原点在拼板坐标中的位置，`rotation` 是绕板原点的旋转。
- **角度**：逆时针为正，写入时归一化到 `[0, 360)`。`Placement.rotation` 是元件在板上的角度，以封装的零度方向为基准。
- **数值精度**：写入保留 4 位小数，小数点固定为 `.`。

拼板 → 机器坐标的变换不在程序里保存，由运行时根据 Mark 点识别结果求出。

## 3. 整体结构

```xml
<SmtProgram formatVersion="1.0">
  <Info/>              程序信息
  <Packages/>          封装：外形尺寸 + 取放/视觉参数
  <Components/>        元件：料号 → 封装
  <Boards/>            单板定义：尺寸、Mark 点、坏板标记、贴片位
  <Panel/>             拼板：板尺寸、拼板 Mark、各子板实例
  <FeederSetup/>       送料器：站位 → 元件，取料位置（与机台相关）
  <Extensions/>        扩展区（可选）
</SmtProgram>
```

各段按上面的顺序写出。读取时不依赖顺序，但所有 id 引用在整个文件读完后统一解析。

`Info`、`Packages`、`Components`、`Boards`、`Panel` 必须各出现一次。`FeederSetup` 和 `Extensions` 是可选段，最多出现一次。没有 `FeederSetup` 的程序可以打开和编辑，便于先离线编程、后配料站。

**写出规则**：长度和角度保留 4 位小数，整数属性（`pinCount`、`speed`、`priority`、`rows`、`columns`、`startIndex`、`feedCount`）写成整数。可选的文本属性为空时不写出，其余属性都写出。`Fiducials`、`BadMarks` 等列表为空时不写出，但 `Packages`、`Components`、`Boards` 即使为空也要写出。

## 4. 元素定义

"必填"列为"是"的属性缺失时读取失败；其余属性缺失时取默认值。

### 4.1 SmtProgram（根）

| 属性 | 必填 | 说明 |
|---|---|---|
| formatVersion | 是 | `主版本.次版本`，见第 6 节 |

### 4.2 Info

| 属性 | 必填 | 默认 | 说明 |
|---|---|---|---|
| name | 是 | | 程序名 |
| productCode | 否 | 空 | 产品编码 |
| revision | 否 | 空 | 程序版次（用户维护，与格式版本无关） |
| createdAt / modifiedAt | 否 | 空 | ISO 8601 时间，例如 `2026-11-06T10:00:00+08:00` |
| author | 否 | 空 | |

子元素 `<Description>` 可选，纯文本。

### 4.3 Packages / Package

| 属性 | 必填 | 默认 | 说明 |
|---|---|---|---|
| id | 是 | | 封装 id，例如 `C0402` |
| bodyLength / bodyWidth / height | 是 | | 本体长（X）、宽（Y）、高，mm |
| pinCount | 否 | 0 | |

子元素：

- `<Pick nozzle="" pickDepth="0" vacuumCheck="true"/>`：`nozzle` 为允许使用的吸嘴型号，多个用逗号分隔；`pickDepth` 为下压量（mm）。
- `<Place placeDepth="0" speed="100"/>`：`speed` 为相对速度百分比（1–100）。
- `<Vision method="bottom" tolerance="0.1"/>`：`method` 取 `none` / `bottom` / `top`。
- `<Extensions>`：可选，见 4.9。

### 4.4 Components / Component

| 属性 | 必填 | 默认 | 说明 |
|---|---|---|---|
| id | 是 | | 元件 id，通常为料号 |
| package | 是 | | 引用 `Package.id` |
| value | 否 | 空 | 例如 `10K`、`100nF` |
| description | 否 | 空 | |

元件只描述"是什么"，物理和取放参数都放在封装上，多个元件共用一个封装时不用重复填写。

### 4.5 Boards / Board

| 属性 | 必填 | 说明 |
|---|---|---|
| id | 是 | 板 id |
| width / height / thickness | 是 | mm |

子元素：

- `<Fiducials>`：板 Mark 点，`<Fiducial id="" x="" y="" shape="circle" size="1.0"/>`。`id`、`x`、`y` 必填；`shape` 取 `circle` / `square` / `cross`，默认 `circle`；`size` 为直径或边长，默认 `1.0`。贴装时用于单板对位。
- `<BadMarks>`：坏板标记位置，`<BadMark id="" x="" y=""/>`，三个属性都必填。运行时检测到该位置有标记则跳过整块板。
- `<Placements>`：贴片位列表。

`Placement` 属性：

| 属性 | 必填 | 默认 | 说明 |
|---|---|---|---|
| id | 是 | | 位号，例如 `R1`，在同一块板内唯一 |
| component | 是 | | 引用 `Component.id` |
| x / y / rotation | 是 | | 板坐标 |
| side | 否 | `top` | `top` / `bottom` |
| enabled | 否 | `true` | `false` 表示不贴（DNP） |

一个程序可以定义多种板（例如阴阳拼板里正反两种板），由拼板实例引用。

### 4.6 Panel

| 属性 | 必填 | 说明 |
|---|---|---|
| width / height | 是 | 拼板外形尺寸，mm |

子元素：

- `<Fiducials>`：拼板 Mark 点，格式同 4.5，坐标为拼板坐标。
- `<BoardInstances>`：子板实例，每块板一条：

| 属性 | 必填 | 默认 | 说明 |
|---|---|---|---|
| id | 是 | | 实例 id，例如 `1`、`2`，界面上即"第几块板" |
| board | 是 | | 引用 `Board.id` |
| x / y / rotation | 是 | | 拼板坐标 |
| enabled | 否 | `true` | `false` 表示整块板不贴 |

`BoardInstance` 下可有 `<Skip placement="R5"/>`，表示这块板上不贴该位号（用于同一拼板中个别板的差异）。

**即使只有一块板，也必须有 `Panel` 和一个 `BoardInstance`。** 运行时只处理"拼板 → 实例 → 贴片位"一条路径，不为单板单独写分支。

拼板实例用逐块列出的方式保存，不保存"行数 × 列数 × 间距"。阵列只是编辑器生成实例时的输入；逐块列出后，个别板的旋转、偏移、禁用都能直接表达。

### 4.7 FeederSetup / Feeder

`FeederSetup` 属性 `machineModel`（可选）：程序针对的机型，换机型时界面提示重新做站位。

`Feeder` 公共属性：

| 属性 | 必填 | 默认 | 说明 |
|---|---|---|---|
| id | 是 | | 送料器 id |
| type | 是 | | `tape` / `tray` / `tube` / `bulk`，以后可增加 |
| slot | 是 | | 机台站位号，字符串，例如 `F1-08` |
| component | 是 | | 引用 `Component.id` |
| pickX / pickY / pickZ | 是 | | 第一个取料位置，机器坐标 |
| rotation | 否 | 0 | 元件在料带/料盘中的角度，与 `Placement.rotation` 共同决定吸嘴旋转量 |
| enabled | 否 | `true` | |
| priority | 否 | 0 | 同一元件有多个送料器时，数值大的优先 |

同一元件可以对应多个送料器（备料、多站位）。

类型专属参数放在与 `type` 同名的子元素里，公共属性不随送料器类型变化：

- `type="tape"`：`<Tape width="8" pitch="4" feedCount="1"/>`。`width`（料带宽，mm）和 `pitch`（料穴中心距，mm）必填；`feedCount` 默认 1。
- `type="tray"`：`<Tray rows="5" columns="10" rowPitch="12" columnPitch="12" startIndex="0"/>`。`startIndex` 默认 0，其余必填。
- `type="tube"`：`<Tube pitch="0"/>`，可省略，`pitch` 默认 0。
- `type="bulk"`：无

`tape` 和 `tray` 的参数子元素必须存在。

以后新增送料器类型时，只需增加新的 `type` 值和对应的子元素。读到不认识的 `type` 时只给警告，不报错退出：公共属性照常读取和校验，子元素原样保留，运行时视为禁用；保存时 `type`、`enabled` 和子元素都原样写回。

### 4.8 运行时状态不保存

已贴数量、当前板号、料带剩余数量等运行时状态不写入程序文件，单独保存，避免"打开程序就产生改动"。

### 4.9 Extensions

`SmtProgram`、`Package`、`Feeder` 下可以有 `<Extensions>`：

```xml
<Extensions>
  <Extension name="com.vendor.optimizer" version="1">
    <!-- 任意 XML -->
  </Extension>
</Extensions>
```

- `name` 使用反向域名，避免冲突。
- 读取器不认识的 `Extension` 原样保留，保存时原样写回。
- 用途：实验功能、第三方工具数据。某个扩展稳定后，在下一个次版本里转成正式元素。

## 5. 校验规则

校验分两个阶段。打开文件时只检查文件本身是否完整、一致；能否在某台机器上生产，到运行前再检查。

### 5.1 id 作用域

| 元素 | 唯一范围 |
|---|---|
| Package、Component、Board、Feeder | 整个程序内，按类别分别唯一 |
| BoardInstance | Panel 内唯一 |
| Placement、BadMark | 所属 Board 内唯一 |
| Fiducial | 所属 Board 或 Panel 内唯一 |
| Skip.placement | 同一 BoardInstance 下不重复 |

不同类别的 id 可以相同，例如封装和元件都叫 `R0603`。id 不能为空，也不能只有空白字符。

### 5.2 打开时校验

以下任一项不通过都是**错误**。读取器报告全部错误（带行号）并拒绝打开：

- XML 语法错误，或根元素不是 `SmtProgram`。
- `formatVersion` 缺失、格式不对，或主版本不受支持（见第 6 节）。
- 必须出现的段缺失，或者某段重复出现（见第 3 节）。`Panel` 下至少要有一个 `BoardInstance`。
- 必填属性缺失；数值不是有限数；整数属性不是整数；布尔值不是 `true` / `false`；枚举值不在列出的范围内（`Feeder.type` 除外）。
- id 违反 5.1 的规则。
- 引用找不到目标，包括 `Component.package`、`Placement.component`、`BoardInstance.board`、`Feeder.component`，以及 `Skip.placement`（要在该实例所引用的板里查找）。引用属性写成空字符串也算找不到目标。
- 数值超出范围：
  - 必须大于 0：封装、板、拼板的尺寸，`Fiducial.size`，料带的 `width` / `pitch`，料盘的 `rows` / `columns` / `rowPitch` / `columnPitch`。
  - 必须大于等于 0：`pinCount`、`Vision.tolerance`、`Tube.pitch`。
  - `Place.speed` 在 1–100 之间，`Tape.feedCount` 至少为 1。
  - `Tray.startIndex` 在 `[0, rows × columns)` 范围内。

以下情况只给**警告**，文件仍然可以打开：

- `Placement` 坐标在板外。有些元件本来就会超出板边。
- 不认识的元素或属性，读取时忽略。如果文件的次版本比读取器高，界面还要提示另存为（见第 6 节）。
- 不认识的 `Feeder.type`（见 4.7）。

角度在读取时归一化到 `[0, 360)`，这不算错误。

实现上分两部分：语法类检查（XML、段、必填属性、数值 / 枚举的写法）在 `ProgramReader` 解析时做；id、引用、数值范围、至少一个子板实例这些一致性规则在 `ProgramValidator` 里，对内存中的 `Program` 检查，读取器解析完调用它并把结果对应回行号。编辑器在接受每次修改前也调用它，所以能保存的程序一定能打开。

### 5.3 运行前校验

以下项不在打开文件时检查，而是在生产开始前，针对本次加工面实际要贴的位号检查：

- 每个要贴的元件至少有一个启用、且类型受支持的送料器。不认识类型的送料器不能供料，它负责的位号必须报出来，不能静默漏贴。
- 吸嘴与封装是否兼容，机台与标定数据是否匹配。

这些检查不通过时，程序仍然可以打开和编辑，只是不能开始生产。

## 6. 版本与兼容规则

`formatVersion` 为 `主版本.次版本`，当前 `1.0`。

**次版本升级（1.0 → 1.1）**：只允许增加内容。

- 新增元素或属性必须是可选的，且有默认值，默认值的行为与旧版本一致。
- 不得删除、改名已有元素或属性，不得改变已有属性的含义或单位。
- 旧读取器打开更高次版本的文件：可以打开，忽略不认识的元素和属性并记录警告；但保存会丢失这些内容，所以界面提示"文件由新版本创建，建议另存为"。

**主版本升级（1.x → 2.0）**：允许不兼容修改。

- 读取器拒绝打开比自身主版本更高的文件，并提示升级软件。
- 读取器打开更低主版本的文件时，按顺序执行迁移（1→2→3…），每一步迁移一个函数并有对应测试。保存时写当前版本，原文件先备份为 `*.bak`。

**修改格式时的流程**：更新本文档 → 按上面规则升版本号 → 更新 `samples/example.smt.xml` → 补读写/迁移测试。
