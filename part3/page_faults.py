import plotille


if __name__ == "__main__":
    touch_count = []
    soft_page_fault = []
    hard_page_fault = []
    with open("page_faults.txt", "r", encoding="utf-8") as f:
        for line in f:
            a, b = line.split(":")
            b, c = b.split(",")
            if int(a) < 128:
                touch_count.append(int(a))
                soft_page_fault.append(int(b))
                hard_page_fault.append(int(c))

    fig = plotille.Figure()
    fig.width = 250
    fig.plot(touch_count, soft_page_fault, label="soft PF")
    fig.plot(touch_count, hard_page_fault, label="hard PF")
    #fig.plot(touch_count, [spf - tc for spf, tc in zip(soft_page_fault, touch_count)], label="extra PF")
    print(fig.show(legend=True))
