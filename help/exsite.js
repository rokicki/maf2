//{{HH_SYMBOL_SECTION
var HH_ChmFilename = "";
var HH_WindowName = "||main";
var HH_GlossaryFont = "";
var HH_Glossary = "0,0,0";
var HH_Avenue = "0,1,1";
var HH_ActiveX = location.protocol=="mk:" || location.protocol=="ms-its:";
//}}HH_SYMBOL_SECTION
var gbKon = false;
var gbNav4 = false;
var gbIE4 = false;
var gbIE = false;
var gAgent = navigator.userAgent.toLowerCase(); 
var gbMac = (gAgent.indexOf("mac") != -1);
var error_count = 0;
var gbsetRS = typeof(document.readyState) == "undefined";

gbIE = (navigator.appName.indexOf("Microsoft") != -1);

{
  gbNav4 = (navigator.appName == "Netscape");
  gbIE4 = (navigator.appName.indexOf("Microsoft") != -1);
}

function msieversion()
{
  var msie = gAgent.indexOf("msie ");
  if (msie > 0)
    return gAgent.substring(msie+5,gAgent.indexOf(";",msie));
  return "";
}

var gbResize = gbIE4 && parseFloat(msieversion()) < 5.5;
var gbFixPre = gbIE4 && parseFloat(msieversion()) < 7.0;


//////////////////////////////////////////////////////////////////////////////////////////////
//
//  Begin DHTML Popup Functions
//
//////////////////////////////////////////////////////////////////////////////////////////////
//variables used to isolate the browser type
var gBsDoc = null;      
var gBsSty = null;
var gBsStyVisShow = null;
var gBsStyVisHide = null;
var gBsClientWidth = 640;
var gBsClientHeight = 480;
var gBsBrowser = null;

//the browser information itself
function _BSPSBrowserItself()
{
  var agent = navigator.userAgent.toLowerCase();
  this.major = parseInt(navigator.appVersion);
  this.ns = ((agent.indexOf('mozilla') != -1) && ((agent.indexOf('spoofer') == -1) && (agent.indexOf('compatible') == -1)));
  this.ns4 = ((this.ns) && (this.major >= 4));
  this.ie = (agent.indexOf("msie") != -1);
  this.ie4 = ((this.ie) && (this.major >= 4));
  if (document.all)
    gBsDoc = "document.all";
  else
    gBsDoc = "document";
  
  if (document.all || document.getElementById)
  {
    gBsSty = ".style";
    gBsStyVisShow = "visible";
    gBsStyVisHide = "hidden";
  }
  else // Netscape 4
  {
    gBsSty = "";
    gBsStyVisShow = "show";
    gBsStyVisHide = "hide";
  }
}

//Here is the browser type 
function _BSPSGetBrowserInfo()
{
  gBsBrowser = new _BSPSBrowserItself();
}

//Get client size info
function GetClientSize()
{
  if (window.innerWidth)
  {
    gBsClientWidth = innerWidth;
    gBsClientHeight = innerHeight;
  }
  else
  {
    // in IE6 standard compliant mode document.body.clientHeight and width are not full size of window
    var el = document.documentElement;
    if (el && el.clientHeight)
    {
      gBsClientWidth = el.clientWidth;
      gBsClientHeight = el.clientHeight;
    }
    else
    {
      el = document.body;
      if (el && el.clientHeight)
      {
        gBsClientWidth = el.clientWidth;
        gBsClientHeight = el.clientHeight;
      }
    }
  }
}

var gstrPopupID = 'BSSCPopup';
var gstrPopupShadowID = 'BSSCPopupShadow';
var gstrPopupTopicID = 'BSSCPopupTopic';
var gstrPopupIFrameID = 'BSSCPopupIFrame';
var gstrPopupIFrameName = 'BSSCPopupIFrameName';

var gstrPopupSecondWindowName = 'BSSCPopup';

var gPopupDiv = null;
var gPopupDivStyle = null;
var gPopupShadow = null;
var gPopupTopic = null;
var gPopupIFrame = null;
var gPopupIFrameStyle = null;
var gPopupWindow = null;
var hotX = 0;
var hotY = 0;

var gbPopupTimeoutExpired = false;

if (BSSCPopup_IsPopup()) 
  document.write("<base target=\"_parent\">");

if (gbFixPre)
  document.writeln("<STYLE type='text/css'> PRE { width:1ex;}</STYLE>");


function BSSCPopup_IsPopup()
{

  return this.name == gstrPopupIFrameName || this.name == gstrPopupID;
}


function NonIEPopup_HandleBlur(e)
{
  window.gPopupWindow.focus();
}

function NonIEPopup_HandleClick(e)
{
  // Because navigator will give the event to the handler before the hyperlink, let's
  // first route the event to see if we are clicking on a Popup menu in a popup.
  document.routeEvent(e);

  // Close the popup window
  if (e.target.href != null) 
  {
    window.location.href = e.target.href;
    if (e.target.href.indexOf("BSSCPopup") == -1) 
      this.close();
  } 
  else 
    this.close();
  
  return false;
}

function BSSCPopup_AfterLoad()
{
  if ((window.gPopupIFrame.document.readyState == "complete") &&
      (window.gPopupIFrame.document.body != null)) 
    BSSCPopup_ResizeAfterLoad();
   else 
     setTimeout("BSSCPopup_AfterLoad()", 200);
}


function BSSCPopup_ResizeAfterLoad()
{
  window.gPopupDivStyle.visibility = gBsStyVisHide;
  // Determine the width and height for the window
  GetClientSize();
  var size = new BSSCSize(gBsClientWidth, gBsClientHeight);
  if (!gbKon)
  {
    window.gPopupIFrameStyle.width = "auto";
    window.gPopupIFrameStyle.height = "auto";
  }
  else
  {
    window.gPopupIFrameStyle.width = 800 + "px";
    window.gPopupIFrameStyle.height = gBsClientHeight/2 + "px";
  }
  window.gPopupShadow.style.width =  "0px";
  window.gPopupShadow.style.height = "0px";
  window.gPopupDivStyle.left = "0px";
  window.gPopupDivStyle.top = "0px";
  window.gPopupDivStyle.width = (gBsClientWidth > 830 ? 800 : gBsClientWidth-30) + "px";
  window.gPopupDivStyle.height = "auto";
  BSSCGetContentSize(gPopupIFrame, size);
  var nWidth = size.x;
  var nHeight = size.y;
  if (nWidth > gBsClientWidth) 
  {
    // Adjust the height by 1/3 of how much we are reducing the width
    var lfHeight = 1.0;
    lfHeight = (((nWidth / (gBsClientWidth - 20.0)) - 1.0) * 0.3333) + 1.0;
    lfHeight *= nHeight;
    nHeight = lfHeight;
    nWidth = gBsClientWidth - 20;
  }
  if (nHeight > gBsClientHeight * .5) 
    nHeight = gBsClientHeight / 2;
  window.gPopupDivStyle.width = "auto"; 
  window.gPopupDivStyle.height = "auto";

  // Determine the position of the window
  var nClickX = window.hotX;
  var nClickY = window.hotY;
  var nTop = 0;
  var nLeft = 0;
  var realScrollTop = document.body.scrollTop;
  var realScrollLeft = document.body.scrollLeft;


  if (document.documentElement && document.documentElement.scrollTop)
  {
    realScrollTop += document.documentElement.scrollTop;
    realScrollLeft += document.documentElement.scrollLeft;
  }

  if (nClickY + nHeight + 20 < gBsClientHeight + realScrollTop) 
    nTop = nClickY + 10;
  else 
    nTop = (realScrollTop + gBsClientHeight) - nHeight - 25;
  
  if (nClickX + nWidth < gBsClientWidth + realScrollLeft) 
    nLeft = nClickX;
  else 
    nLeft = (realScrollLeft + gBsClientWidth) - nWidth - 25;
  if (nTop <0) 
    nTop = 1;
  if (nLeft<0) 
    nLeft = 1;

  window.gPopupDivStyle.left = nLeft + "px";
  window.gPopupDivStyle.top = nTop + "px";
  window.gPopupIFrameStyle.width = window.gPopupShadow.style.width = window.gPopupTopic.style.width = nWidth +   "px";
  window.gPopupIFrameStyle.height = window.gPopupShadow.style.height = window.gPopupTopic.style.height = nHeight +"px";
  window.gPopupDivStyle.visibility = gBsStyVisShow;
  setTimeout("BSSCPopup_Timeout();", 100);
  return false;
}


function  BSSCSize(x, y)
{
  this.x = x;
  this.y = y;
}

function BSSCGetContentSize(thisWindow, size)
{

  /* This function attempts to find out the optimal size for a window, by first discovering how tall it needs
     to be and then reducing the width until the height starts to increase. Unfortunately this is not 100%
     successful - some browsers will add an unnecessary scroll bar for no apparent reason if the window is very 
     small, which is why I add a fair amount of extra allowance at the end. And some browsers don't update the
     size of the content quickly enough.  However, the window usually seems to come out in a reasonable size 
     in all browsers tried: IE5+,Opera 7,Mozilla 1.3+, Konqueror */
  var y = 1;
  var x = 800;
  var myDoc = thisWindow.document.body;
  myDoc.style.width = (gBsClientWidth > 830 ? 800 : gBsClientWidth-30) + "px";
  myDoc.style.height = "auto";
  myDoc.style.margin="0px";
  myDoc.style.padding="0px";
  if (gbResize)
  {
    thisWindow.resizeTo(800,1);
    thisWindow.resizeTo(800,myDoc.scrollHeight);
    thisWindow.resizeTo(800,myDoc.scrollHeight);
  }
  var miny = myDoc.scrollHeight;
  size.x = 800;
  size.y = miny;

  for (i = 15; i > 0; i--) 
  {
    size.x = x = i * 50;
    myDoc.style.width = x + "px";
    if (gbResize)
      thisWindow.resizeTo(size.x,miny);
    if (myDoc.scrollHeight > miny || myDoc.scrollWidth > x) 
    {
      x = (i + 1) * 50;
      myDoc.style.width = x + "px";
      break;
    }
  }
  myDoc.style.padding = "0.5ex";
  size.x = x;
  if (!gbKon)
  {
    myDoc.style.width = x + "px";
    size.x = myDoc.scrollWidth + 30;
  }
  size.y = myDoc.scrollHeight + 15;
  myDoc.style.width = "auto";
  if (size.y < 100) 
    size.y = 100;
}



function BSSCPopupParentClicked()
{
   BSSCPopupClicked();

  return;
}


function BSSCPopupClicked()
{
  if (!window.gbPopupTimeoutExpired) 
    return false;
  
  if (gPopupIFrame.document) 
    gPopupIFrame.document.body.onclick = null;
  
  document.onclick = null;
  document.onmousedown = null;

  // Simply hide the popup
  gPopupDivStyle.visibility = gBsStyVisHide;
  gPopupDivStyle.width = "0px";
  gPopupDivStyle.height = "0px";
  gPopupDivStyle.left = "0px";
  gPopupDivStyle.top = "0px";
  return true;
}


function BSSCHidePopupWindow()
{
  if (window.gPopupWindow != null) 
  {
    if (gBsBrowser.ns4) 
    {
      if ((typeof window.gPopupWindow != "undefined") && (!window.gPopupWindow.closed)) 
      {
        window.gPopupWindow.close();
        window.gPopupWindow = null;
      }
    }
  }

  return;
}

var gbPopupMenuTimeoutExpired = false;

/////////////////////////////////////////////////////////////////////
function BSSCPopup_ClickMac()
{
// Left over from original version - no idea if really required.
  if (!gPopupDiv)
  {  
    var bClickOnAnchor = false;
    var el;
    if (window.event != null && window.event.srcElement != null)
    {
      el = window.event.srcElement;
      while (el != null)
      {
        if ((el.tagName == "A") || (el.tagName == "AREA"))   
        {
          bClickOnAnchor = true;
          break;
        }
        if (el.tagName == "BODY") 
          break;
        el = el.parentElement;
      }
    }
    if (BSSCPopup_IsPopup())
    {
      if (!bClickOnAnchor) 
      {
        parent.window.gPopupWindow = null;
        self.close();
      }
    }
    else
    {
      bClosePopupWindow = true;
      if (bClickOnAnchor && el.href && el.href.indexOf("javascript:BSSCPopup") != -1)
        bClosePopupWindow = false;
      if (bClosePopupWindow)
      {
        if (window.gPopupWindow != null)
        {
          var strParam = "titlebar=no,toolbar=no,status=no,location=no,menubar=no,resizable=yes,scrollbars=yes,height=300,width=400";
          window.gPopupWindow = window.open("", gstrPopupSecondWindowName,strParam);
          window.gPopupWindow.close();
          window.gPopupWindow = null;
        }
      }
    }
  }
 }

//////////////////////////////////////////////////////////////////////

_BSPSGetBrowserInfo();



function _BSSCOnLoad()
{
  // Ensure images with no title get some kind of tooltip
  for (i= 0; i < document.images.length;i++)
  {
    im = document.images[i];
    if (im.title=="")
      im.title = im.alt;
  }
  if (window.frames.length >= 1)
  {
    gPopupIFrame = window.frames[gstrPopupIFrameName];
    gPopupDivStyle = eval("gPopupDiv" + gBsSty);
    if (document.getElementById)
      gPopupIFrameStyle = document.getElementById(gstrPopupIFrameID).style;
    else
      gPopupIFrameStyle = eval(gBsDoc + "['" + gstrPopupIFrameName + "']" + gBsSty);
  }
  // implement "niceborder" behavior - no longer using IE behaviors for this
  if (document.getElementsByTagName)
  {
    var tableCol = document.getElementsByTagName("TABLE");
    for (i = 0; i < tableCol.length;i++)
      if (tableCol[i].className == "niceborder")
      {
        var rowCol = tableCol[i].rows;
        for (j = 1; j < rowCol.length;j +=2)
          rowCol[j].className = "darkrow";
      }
  }
}

function _BSSCOnUnload()
{
}


function HasExtJs()
{
  if (!document.all && !document.getElementById)
    return false;
  if (typeof (_BSSCOnLoad) == "undefined")
    return false; 
  return true;
}

function BSSCOnLoad()
{
  if (HasExtJs()) {_BSSCOnLoad();}
}

function BSSCOnUnload()
{
  if (HasExtJs()) { _BSSCOnUnload(); }
}
