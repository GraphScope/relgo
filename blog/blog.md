[name]: RelGo
[gopt]: GOpt

## 1. 背景

在数据管理和分析领域，关系型数据库一直是结构化数据存储和检索的基石，支撑了大量现实应用，在金融、医疗、物流等场景中承担着至关重要的作用。为了方便数据库使用者使用数据库进行创建和检索等任务，结构化查询语言SQL（Structured Query Language）应运而生，且已被各种关系行数据库管理系统（Relational Database Management System, RDBMS）广泛采用。以下是使用SQL查询在IMDB数据上进行查询的一个例子：

```
查询1: Q1

SELECT n.name 
FROM NAME AS n1,
     CAST_INFO AS ci1,
     CAST_INFO AS ci2,
     TITLE AS t1,
     TITLE AS t2,
     MOVIE_COMPANIES AS mc1,
     MOVIE_COMPANIES AS mc2,
     COMPANY_NAME AS cn
WHERE n1.id = ci1.person_id
      AND ci1.movie_id = t1.id
      AND t1.id = mc1.movie_id
      AND mc1.company_id = cn.id
      AND n1.id = ci2.person_id
      AND ci2.movie_id = t2.id
      AND t2.id = mc2.movie_id
      AND mc2.company_id = cn.id
      AND t1.title <> t2.title;
```

IMDB中与该查询相关的表之间的关联如以下ER图所示：
![IMDB数据集部分ER图](imdb.svg "图1: IMDB数据集部分ER图")
其中，NAME表和TITLE表中分别存储了人和电影的相关信息，CAST_INFO存储了人/电影/角色（在电影中扮演的角色）/职业（例如演员、导演、编剧等）之间的四元关系，COMPANY_NAME中存储了电影公司的名字，而MOVIE_COMPANIES表中存的则是电影/电影公司/公司类型三者之间的关联。因此，上述SQL查询查找的是那些参与制作同一个公司至少两部电影的人。
容易发现，使用SQL语句来描述查询内容有时会使得查询语句非常冗长，尤其是当查询中需要描述表与表之间的关系的时候。
另一个更直观的例子是查找n个互相认识的人。由于每次描述两个人之间的互相认识的关系时，就需要增加两个join条件，例如
```
Person1.id = Knows.person1id 
AND Person2.id = Knows.person2id
```
则描述n个互相认识的人至少要写n*(n-1)个join条件。人工去写这样的查询语句显然效率是很低的。

当查询中需要描述表与表之间的关联时，用图查询来进行描述是相对更方便的。其优势在于可以用图模式的方式来描述查询，我们以Cypher为例，查询Q1可以表示为：
```
MATCH (n1:NAME)-[ci1:CAST_INFO]->(t1:TITLE)-[mc1:MOVIE_COMPANIES]->(cn:COMPANY_NAME),
      (n1)-[ci2:CAST_INFO]->(t2:TITLE)-[mc2:MOVIE_COMPANIES]->(cn)
WHERE t1.title <> t2.title
RETURN n1.name;
```
该查询直观地描述了如下图模式：
![Q1对应的图模式](cypher-case.svg "图2: Q1对应的图模式")
对于查找n个互相认识的人的场景，使用图查询时只需要描述包含n个节点的完全图即可（即n-clique）。
上述例子表明，使用图查询能够大大节省数据库使用者写SQL的时间，更快速地表达其想进行的查询。

因此，在ISO SQL:2023中新增了扩展SQL/PGQ (SQL/Property Graph Graph Queries)，以支持在SQL表达式中进行图查询。
假定Q1中的NAME、TITLE和COMPANY_NAME表中每个元组表示一个节点，假定CAST_INFO和MOVIE_COMPANIES表中的每个元组表示一条边，可以将Q1改写为如下SQL/PGQ查询：

```
SELECT n_name FROM GRAPH_TABLE (G
    MATCH (n1:NAME)-[ci1:CAST_INFO]->(t1:TITLE)-[mc1:MOVIE_COMPANIES]->(cn:COMPANY_NAME),
      (n1)-[ci2:CAST_INFO]->(t2:TITLE)-[mc2:MOVIE_COMPANIES]->(cn)
    WHERE t1.title <> t2.title
    COLUMNS (n1.name AS n_name) 
) g;
```

上述SQL/PGQ扩展解决了如何在SQL表达式中写图查询的问题，但是在获取SQL/PGQ的查询语句之后，如何针对其进行分析和优化仍然是SQL/PGQ带来的一个新的问题。
查询优化指的是根据用户给定的查询找到高效的查询计划，以达到更优的查询性能。
关系数据库查询优化器的相关研究工作通常是针对SPJ查询框架（selection-projection-join）开展的。而对于SQL/PGQ语句而言，传统的SPJ查询框架无法直接描述语句中的图查询部分（即GRAPH_TABLE (...)部分）。
因此，难以直接使用传统的关系数据库的优化器来优化SQL/PGQ语句。
对于该问题，我们首先重新定义了查询框架，随后基于GOpt设计并实现了名为RelGo的统一优化框架。
我们的主要贡献可以总结如下：

1. 我们定义RGMapping来根据SQL/PGQ的语句将关系数据模型映射到属性图模型。基于RGMapping，我们定义了新的查询框架SPJM。SPJM能被用于分析同时包含关系和图查询的查询表达时。
2. 我们构建了将SPJM查询转化为SPJ的相关理论。基于该理论，可以将SPJM查询转化为SPJ查询，从而使得现有关系数据库能直接处理SPJM查询。这种方法称为图无关方法（graph-agnostic approach）。我们证明了这种图无关的方法在一些场景下有巨大的搜索空间（指数级地大于我们的方法的搜索空间）。
3. 我们设计并实现了RelGo以同时利用关系数据库中的查询优化器（简称为关系优化器）和图数据库中的查询优化器（简称为图优化器）来优化SPJM查询。RelGo使用了当前最优的图优化技术，并基于图索引实现了图相关的物理操作。
4. 我们将RelGo和现有的工业级关系优化框架Calcite相结合，并使用DuckDB作为执行引擎。在LDBC benchmark上的实验结果表明在使用图索引的情况下，RelGo相比于图无关方法能取得平均21.9倍的速度提升。当基准方法能够使用图索引时，RelGo仍然能取得平均5.4倍的提升。

## 2. 数据模型和SPJM查询框架
我们先简单说明本文会用到的一些概念，包括属性图（Property Graph）和RGMapping。

### 2.1 数据模型
给定一个同时包含关系查询和图查询的表达式中，为了能够将两者的查询计划整合起来，需要定义关系数据与图数据之间的转化方式。因此，我们给出了如下定义。

<u>**属性图**</u>
被定义为$G = (V_G, E_G)$，其中$V$表示节点集，$E \subseteq V \times V$表示图中的边集。$e = (v_s, v_t)$表示一条从节点$v_s \in V$到$v_t \in V$的边。
对于每个图中的节点或边（统称为图元素$\epsilon$），我们用id($\epsilon$)和l($\epsilon$)分别表示$\epsilon$的唯一ID和标签，用$\epsilon.a_i$表示$\epsilon$的属性$a_i$的值。


<u>**RGMapping**</u>
将关系表中的元组映射到属性图中的节点或边。
具体地，RGMapping包含一个节点映射和一个边映射，它们分别将元组映射到不同的节点和不同的边。

为了更方便地描述RGMapping的定义，我们结合如下图来进行叙述。

![RGMapping的例子](rgmapping.svg "图3： RGMapping的例子")

该图中包含四个关系表，分别为Knows, Person, Likes和Message。它们之间的关联可以用图中的ER图进行描述。
具体地，ER图中包含实体和关联，而表示实体的那些关系表中的元组可以被映射为图中的节点，表示关联的关系表中的元组则可被映射为图中的边。
该例子中，表Person和Message中的元组被映射为节点，表Knows和Likes中的元组被映射为边。
RGMapping就是用来定义这种映射关系的。
为便于说明，后文称这两类表分别映射到节点和边的表为点表和边表。
对于边表而言，仅仅知道一个元组和边之间的映射关系是不够的，还需要知道边的起点和终点与点表之间的关联关系。
具体来说，针对该例子中的边表$R_{Likes}$，我们能给出如下两个关联关系：
$$
\lambda_{Likes}^s: R_{Likes} \rightarrow R_{Person} （R_{Likes}.pid = R_{Person}.person_id）
$$
$$
\lambda_{Likes}^t: R_{Likes} \rightarrow R_{Message}  R_{Likes}.mid = R_{Message}.message_id
$$
这两个关联关系分别表明了，对于$R_{Likes}$中的一个元组，其对应属性图中的一条边，边的起点与点表$R_{Likes}$中的一个元组对应，终点与点表$R_{Message}$中的一个元组对应。
元组间的对应关系根据属性$R_{Likes}.pid$, $R_{Person}.person_id$和$R_{Message}.message_id$的值判断。
例如，Likes表中的元组与$R_{Person}$中的元组之间有关联，当且仅当$R_{Likes}$.pid = $R_{Person}$.person_id。
通常来说，可以根据关系表之间的主外键关联来给出上述关联关系。
具体地，因为Likes表中有指向$R_{Person}$和$R_{message}$的外键pid和mid，所以Likes关联的点表是$R_{Person}$和$R_{message}$。


知道如上信息后，即可进行关系表与属性图中节点和边的相互转化。

### 2.2 SPJM查询框架定义
为了表示图查询，我们引入了新的操作符$\mathcal{M}$，称为匹配操作（Matching Operator）。该操作符接受一个图关系表$GR$和一个模式图$\mathcal{P}$作为输入，以一个图关系表作为输出。

这里，图关系表是一类特殊的表。该表中的每个元组，表示一个属性图，元组的每个属性的值为节点或边。
对于$GR$中的每个属性图，匹配操作$\mathcal{M}(GR, \mathcal{P})$查找其所有与$\mathcal{P}$同构的子图。在$\mathcal{M}(GR, \mathcal{P})$返回的图关系表的每一行对应一个这样找到的同构子图。为了方便描述，将$\mathcal{M}(GR, \mathcal{P})$简记为$\mathcal{M}(\mathcal{P})$。

给定一系列关系表以及基于RGMapping构造的属性图$G$后，结合匹配操作，我们可以如下定义SPJM查询：
$$
Q = \pi_A(\sigma_\Psi(R_1 \Join \cdots \Join R_m \Join (\widehat{\pi}_{A*}\mathcal{M}_G(\mathcal{P}))))
$$
其中，$\widehat{\pi}_{A*}\mathcal{M}_G(\mathcal{P})$表示图查询部分，$\widehat{\pi}$将图关系表中的节点和边映射到它们的属性，即将图关系表映射为关系表，该操作对应SQL/PGQ中的COLUMNS关键字。

## 3. 优化匹配操作
本节中我们具体讨论如何对匹配操作进行优化。具体地，可以从逻辑变换（logical transformation）和物理实现（physical implementation）两方面进行优化。
逻辑变换优化需要将匹配操作转化为性能更高的逻辑等价的表达，物理实现优化则关注如何高效执行匹配操作。

### 3.1 逻辑变换
考虑匹配操作$\mathcal{M}(\mathcal{P})$，其中$\mathcal{P}$描述的模式图为
```
MATCH (p1:Person)-[e1:Knows]->(p2:Person)-[e3:Likes]->(m:Message),
      (p1)-[e2:Likes]->(m)
RETURN p1.name, p1.place_id, p2.name
```
该模式图如图3(b)中所示，查找两个喜欢相同Message的人。
针对该查询，我们提出了两种优化方法，一种是图无关方法（graph-agnostic approach），另一种是图相关方法（graph-aware approach）。

#### 3.1.1 图无关方法
图无关方法根据RGMapping直接将模式图转化为表之间的join，从而将SPJM问题转化为SPJ问题，再使用现有的关系优化器进行查询优化。
根据第2.1节中描述的RGMapping以及图3中描述这些关系表之间关系的ER图，容易得到$R_{Person}$, $R_{Message}$, $R_{Knows}$和$R_{Likes}$分别映射到图中标签为Person, Message, Knows和Likes的点或边。
并且，点表和边表之间存在如下关联关系：
$$
\lambda_{Likes}^s: R_{Likes} \rightarrow R_{Person} （R_{Likes}.pid = R_{Person}.person\_id）
$$
$$
\lambda_{Likes}^t: R_{Likes} \rightarrow R_{Message} （R_{Likes}.mid = R_{Message}.message\_id）
$$
$$
\lambda_{Knows}^s: R_{Knows} \rightarrow R^1_{Person} （R_{Knows}.p1d = R^1_{Person}.person\_id）
$$
$$
\lambda_{Knows}^t: R_{Knows} \rightarrow R^2_{Person} （R_{Knows}.p2d = R^2_{Person}.person\_id）
$$
其中，$R^1_{A}$和$R^2_{A}$均表示A表，上标仅用来区分边的起点和终点。

模式图中的每一条边都可以通过点表和点表之间的join得到。
具体地，上述模式图中的边可以转化为如下join：
$$
R'_1 = R^1_{Person} \Join_{person\_id = pid1} R_{Knows} \Join_{pid2 = person\_id} R^2_{Person}
$$
$$
R'_2 = R^1_{Person} \Join_{person\_id = pid} R^1_{Likes} \Join_{mid = message\_id} R_{Message}
$$
$$
R'_3 = R^2_{Person} \Join_{person\_id = pid} R^2_{Likes} \Join_{mid = message\_id} R_{Message}
$$
因此，上述匹配操作可以转化为$R'_3 \Join R'_2 \Join R'_1$从而使用关系优化器进行查询优化。

#### 3.1.2 图相关方法
本节我们提出一种图相关方法，从而用图优化的思想来对匹配操作进行优化。
具体地，我们首先拆分模式图，得到一棵分解树。
![分解树的例子](rgmapping.svg "图4： 分解树的例子")
具体地，分解树中的每个节点都表示模式图或其导出子图。
分解数的根节点是原模式图$\mathcal{P}$，对于树中的每个中间节点，其有两个孩子节点，满足两个孩子节点都是该中间节点的子图。
并且将两个孩子节点join起来即得到该中间节点。
分解树的叶子节点必须为最小匹配单元（Minimum Matching Component, MMC）。
MMC可以是仅包含一个节点的模式子图，也可以是一个完全星型模式子图$\mathcal{P}(u; V_s)$，其中$u$是星型的中心，而$V_s$是该星型结构的叶子节点构成的集合。
给定一个中间节点，当星型模式子图是其右孩子，且星型模式子图的叶子节点均在其左孩子中出现时，才称该星型模式子图为完全星型模式子图。
容易发现，仅包含一条边的模式子图是一种特殊的完全星型模式子图。
如上设计主要是为了保证执行计划是最坏情况最优的（worst-case optimal）。