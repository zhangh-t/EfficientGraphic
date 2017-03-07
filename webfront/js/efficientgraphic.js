/*global $ */

var chartCount = 0;

/*echart 图表数据*/
var openedFiles = null;
var globalOption = {}
var GraphicInstance = null;

function onChange(files){
    if (files.length > 0){
        $("#chart-container").html("");
        if (window.FileReader){
            var reader = new FileReader;
            reader.onload = function () {
                var jsonAsString = this.result;
                fromJSONToChart(JSON.parse(jsonAsString));
            }
            openedFiles = files;
            reader.readAsText(files[0]);      
        }  
    }
 
}
function titleJSON(name) {
    var titlejson = {};
    titlejson.text = name;
    titlejson.left = 'center';
    titlejson.top = 20;
    titlejson.textStyle = {'color' : '#ccc'};
    return titlejson;    
}

function GTJEfficientGraphicSchema(GraphicJson, Graphic) {
    this.GraphicInstance = Graphic;
    this.jsonData = GraphicJson;
    this.level = function () {
        return this.jsonData.lv;
    }
    this.getColorRanderStyle = function (){
        var res = globalOption[this.level()]
        if(res == null){
            return 0;
        }
        else{
            return res._crs;
        }
    }

    this.getGraphicStyle = function () {
        var res = globalOption[this.level()]
        if(res == null){
            return 0;
        }
        else{
            return res._gs;
        }
    }
    this.graphicOption = {
        colorRenderStyle : this.getColorRanderStyle() ,
        graphicStyle : this.getGraphicStyle(),
    }
    this.onClick = function () {}
    this.graphicColorRenderOption = function () {
        return this.graphicOption.colorRenderStyle;
    }

    this.graphicStyle = function () {
        return this.graphicOption.graphicStyle;
    }

    ///////////////////////////////////一些数据获取的函数///////////////////////////////////
    //1. 图表名称
    this.graphicName = function () {
        return this.jsonData._pn;
    }
    //2. 平均
    this.graphicBlockAverage = function (){
        return this.jsonData._pba; //子的总耗时 / 子的总hitcount
    }
    //3. hitcount
    this.graphicHitCount = function () {
        return this.jsonData._ph;
    }
    //4. 最大
    this.graphicMax = function () {
        return this.jsonData._pm;
    }
    //5. 总耗时
    this.graphicTotal = function () {
        return this.jsonData._pt;
    }
    //6. 与基准值的差值
    this.diffToStandard = function () {
        return this.jsonData._dv;
    }
    //7. 获取基准
    this.graphicStandard = function (){
        return this.jsonData._sv;
    }
    //8. 子块
    this.graphicChildren = function() {
        return this.jsonData._pc;  //一个array
    }

    ///////////////////////////////////一些功能性的函数///////////////////////////////////
    //生成颜色
    this.getColorArray = function () {
        var children = this.graphicChildren();
        var resArray = new Array();
        var r = 0x00;
        var g = 0xff;
        var b = 0x00;
        var diffArray = new Array();
        var min = 10000000;
        var max = -1000000;
        for (var i = 0 ; i < children.length; ++i) {
            var diff = children[i]._dv;     //与基准值的差值
            diffArray.push(diff);
            if(diff > max ){
                max = diff;
            }
            if(diff < min) {
                min = diff;
            }
        }
        var range = max - min;
        if (range == 0){
        for(var i = 0 ; i < diffArray.length; ++i){
             resArray.push("rgba(0, 236,22, 0.8)");
            }
        }
        else{
         var step = 5;
         var resArray = new Array();
         for(var i = 0 ; i < diffArray.length; ++i){
                var percentage = (diffArray[i] - min) / range * 100;
                _r = r + parseInt(step * percentage);
                if (_r > 255) {_r = 255;}
                _g = g - parseInt(step * percentage);
                if(_g < 0) {_g = 0;}
                resArray.push("rgba(" + _r + "," + _g + "," +  "0, 0.8)");
            }
        }
        return resArray;
    }
    //判断是否为叶子
    this.leafNode = function () {
        var childrenList = this.graphicChildren();
        return childrenList == null || childrenList.length == 0;
    }
    //获取饼图上的数据
    this.getData = function () {
        //...
        var dataList = new Array;
        var childrenList = this.graphicChildren();
        if(this.leafNode()){
            dataList.push({'value' : 100, 'name' : this.graphicName()});
        }
        else{
            for (var i = 0 ; i < childrenList.length; ++i){
            var item = {};
            item.value = childrenList[i]._pt;
            item.name = childrenList[i]._pn;
            dataList.push(item);
            }
        }
        return dataList;
    }
    //获取series json
    this.getSeries = function (graphicType) {
        var seriesJSON = {};
        if(graphicType == 0) {
            //饼图
            seriesJSON.name = this.graphicName();
            seriesJSON.type = "pie";
            seriesJSON.radius = "60%";
            seriesJSON.center = ['50%', '50%'];
            seriesJSON.roseType = "angle";
            seriesJSON.data = this.getData();
            seriesJSON.label = {"normal": {"textStyle": {"color": 'rgba(255, 255, 255, 0.3)'}}};
            seriesJSON.itemStyle = {"normal" : {'shadowBlur' : 200, 'shadowColor' : 'rgba(0, 0, 0, 0.5)'}};
            seriesJSON.animationType = 'scale';
            seriesJSON.animationEasing = 'elasticOut';
            seriesJSON.animationDelay = function (idx) {
                    return Math.random() * 200;
                }
             //默认
            var renderOption = this.graphicColorRenderOption();
            if( renderOption == 0){
                seriesJSON.itemStyle.normal.color = '#c23531';
            }
            else{
                //生成颜色
                var colorList = this.getColorArray();
                seriesJSON.color = colorList;
            }
        }
        else if(graphicType == 1){
            //散点图
        }
        else if(graphicType == 2){
            //雷达 没实现
        }
        return seriesJSON;
    }

    this.generateCommon = function () {
        var optionCommon = {};
        optionCommon.backgroundColor = '#2c343c';
        optionCommon.title = titleJSON(this.graphicName());
        optionCommon.tooltip = {'trigger': 'item', 'formatter': this.toolTipCallBack};
        return optionCommon;
    }
    this.generatePie = function (){
        var optionJSON = this.generateCommon();
        optionJSON.series = this.getSeries(0);
        return optionJSON;
    }

    this.generateScatter = function () {
        var optionJSON = this.generateCommon();
        optionJSON.backgroundColor = '#ffffff';
        var xyList = new Array();
                    //获取最大值
                    function getMax (list) {
                       res = 0.0;
                       for (var i = 0 ; i < list.length; ++i){
                          var total = list[i]._pt
                          if(total > res){
                              res = total;
                           }
                          xyList.push([i, total]);
                        }
                    }
        optionJSON.grid = {x : '10%', y : '10%', x2 : '10%', y2 : '10%'};
        var children = this.graphicChildren();
        optionJSON.xAxis = {gridIndex : 0, min : 0, max : children.length};
        optionJSON.yAxis = {gridIndex : 0, min : 0, max : getMax(children)};
        //基准线
        var standardValue = this.graphicStandard();
        var markLineHorizon = {
             animation: false,
             data: [[{
                   coord: [0, standardValue],
                   symbol: 'none'
             }, {
                   coord: [children.length, standardValue],
                   ymbol: 'none'
                   }]],
                 tooltip: {
                 formatter: 'average :' + standardValue
                 },
             };
        optionJSON.series = {
               name : this.graphicName(),
               type : 'scatter',
               xAxisIndex: 0,
               yAxisIndex: 0,
               data : xyList,
               markLine : markLineHorizon
        };
        return optionJSON;
    }
    this.generateRader = function () {
        //雷达图没实现
        return this.generatePie();
    }

    this.generateOption = function () {
        var res = {};
        if(this.graphicStyle() == 0) {
            res = this.generatePie();
        }
        else if(this.graphicStyle() == 1){
            res = this.generateScatter();
        }
        else if(this.graphicStyle() == 2){
            res = this.generateRader();
        }
        return res;
    }
    //生成饼图选项
    this.generateOptions = function () {
        var optionJSON = this.generateOption();
        return optionJSON;
    }
/////////////////////////////////////一些echarts的回调函数/////////////////////////////////////
    //自定义提示
    var Graphic = this;
    this.toolTipCallBack = function (param) {
         function generateToolTipJson(graphicSchema){
              var res = {};
              res.name = graphicSchema.graphicName();   //名字
              res.total = graphicSchema.graphicTotal();  //总耗时
              res.diffToStandard = graphicSchema.diffToStandard();
              res.max = graphicSchema.graphicMax();
              res.hitCount = graphicSchema.graphicHitCount();
              return res;
         }
         var toolTipJson = {}
         var index = param.dataIndex;
         var schema = new GTJEfficientGraphicSchema(Graphic.graphicChildren()[index]);
         var isLeaf = Graphic.leafNode();
         if(isLeaf){
            //如果是叶子节点
            return "";
         }
         else{
            toolTipJson = generateToolTipJson(schema);
         }
         //
         var urBegin = "<ur style='text-align:left'>" ;
         var nameLi = "<li> 名称 : " + toolTipJson.name + "</li>";
         var totalLi = "<li>总耗时(ms) : " +toolTipJson.total + "</li>"
         var selfAverageLi = "<li> 基准差值(ms) : "  + toolTipJson.diffToStandard + "</li>";
         var maxLi = "<li> 单次最大(ms) : " + toolTipJson.max + "</li>";
         var hitCountLi = "<li> 命中次数 : " + toolTipJson.hitCount + "</li>";
         var urEnd = "</ur>";
         var res = nameLi + totalLi + selfAverageLi
                + maxLi + hitCountLi;
         return urBegin + res + urEnd;
    }
    //点击回调
    var thisGraphicSchema = this;
    this.clickCallback = function (param) {
        if(param.componentType != 'markLine') {
                var index = param.dataIndex;
                var children = thisGraphicSchema.graphicChildren();
                //1. 切换
                var childrenDataJSON = children[index];
                var schema = new GTJEfficientGraphicSchema(childrenDataJSON, thisGraphicSchema.GraphicInstance);
                thisGraphicSchema.GraphicInstance.switchSchema(schema, true);
                //2. 让返回按钮生效
                thisGraphicSchema.GraphicInstance.backWardBtn.attr("disabled", false);
        }
    }
}

/*一个echart控件*/
function GTJEfficientGraphic (GraphicJson){
    //容器
    this.container = $("#chart-container");
    this.backWardBtn = null;
    //图表
    this.eChart = null;
    //数据结构容器
    this.schemaStack = [];
    this.curSchema = null;
    this.onReturn = function () {
        if(GraphicInstance.schemaStack.length != 0){
            var lastSchema = GraphicInstance.schemaStack.pop();
            GraphicInstance.switchSchema(lastSchema, false);
        }
        if(GraphicInstance.schemaStack.length == 0){
            //返回键失效
        }
    }
    //切换图表
    var returnBtn = null;
    this.switchSchema = function (schema, bPush) {
        thisGraphicSchema = schema;
        this.eChart.clear();
        this.eChart.setOption(schema.generateOptions());
        if(this.curSchema != null){
            this.eChart.off('click', this.curSchema.clickCallback);
        }
        this.eChart.on('click', schema.clickCallback);
        if(bPush){
             if(this.curSchema != null){
                 this.schemaStack.push(this.curSchema);
             }
        }
        this.curSchema = schema;
        if(this.schemaStack.length > 0) {
            $(".btn-primary").removeClass("disabled");
        }
        else{
            $(".btn-primary").addClass("disabled");
        }
    }

    //1. 构造
    this.constructor = function () {
        var schema = new GTJEfficientGraphicSchema(GraphicJson, this);
        var name = schema.graphicName();
        var newDiv = "<div class = 'row'>" + "<div id = '" + name + "'" + " style = 'width:100%;height:600px'>" + "</div>";
        var backWardID = name+"_backward";
        returnBtn = $("#"+backWardID);
        var backward = "<div class = 'row'><div class = 'col-md-3'><a class = 'btn btn-primary disabled' id = '"+ backWardID + "'>Return</a></div></div>";
        newDiv += backward;
        newDiv += "</div>";
        this.container.append(newDiv);
        this.backWardBtn = $("#" + backWardID);
        this.backWardBtn.bind("click", this.onReturn);
        this.eChart = echarts.init(document.getElementById(name));
        this.switchSchema(schema, true);
    }
    this.constructor();

};

var graphicList = new Array();
function fromJSONToChart(Json){
    if (Json != null){
      var report = Json.report;
      globalOption = Json.globalOption;
      var item = report[0];
      //单例
      var graphic = new GTJEfficientGraphic(item);
      GraphicInstance = graphic;
      graphicList.push(graphic);
    }
};

$(document).ready(function (){

});