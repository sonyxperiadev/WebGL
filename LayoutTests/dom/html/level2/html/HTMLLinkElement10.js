

   /**
    *  Gets URI that identifies the test.
    *  @return uri identifier of test
    */
function getTargetURI() {
      return "http://www.w3.org/2001/DOM-Test-Suite/level2/html/HTMLLinkElement10";
   }

var docsLoaded = 0;
var mainLoaded = false;
var prefetchLoaded = false;

function finishTest() {
    if (mainLoaded && prefetchLoaded) {
        setResult(null, null);
    } else {
        if (!prefetchLoaded) {
            setResult("fail", "No prefetch onload fired");
        } else {
            setResult("fail", "Prefetch fired, but maybe the document onload didn't");
        }
    }
    if (window.layoutTestController) {
        layoutTestController.notifyDone();
    }
}

function loadComplete() {
    mainLoaded = true;
    if (++docsLoaded == 2) {
        finishTest();
    }
}

function prefetchComplete() {
    prefetchLoaded = true;
    if (++docsLoaded == 2) {
        finishTest();
    }
}



function startTest10() {
    if (window.layoutTestController) {
        layoutTestController.dumpAsText();
        layoutTestController.waitUntilDone();
    }
}
