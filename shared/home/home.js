/* HomeMaster homepage v2 — live prices in catalog cards (current website pricelist, as shown on product pages) */
(function () {
  function fmt(v, cur) {
    var s = Number(v).toFixed(2).split('.');
    s[0] = s[0].replace(/\B(?=(\d{3})+(?!\d))/g, ' ');
    return s.join('.') + ' ' + (cur || '');
  }
  function load(el) {
    var id = parseInt(el.getAttribute('data-tmpl'), 10);
    if (!id) return;
    fetch('/website_sale/get_combination_info', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      credentials: 'same-origin',
      body: JSON.stringify({ jsonrpc: '2.0', method: 'call', params: { product_template_id: id, product_id: false, combination: [], add_qty: 1 } })
    }).then(function (r) { return r.json(); }).then(function (d) {
      var x = d && d.result; if (!x || x.price == null) return;
      var cur = (x.product_tracking_info && x.product_tracking_info.currency) || '';
      el.textContent = fmt(x.price, cur);
      if (x.free_qty > 0) { var s = document.createElement('small'); s.textContent = 'in stock'; el.appendChild(s); }
    }).catch(function () {});
  }
  function run() { document.querySelectorAll('.hmh [data-tmpl]').forEach(load); }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', run); else run();
})();
