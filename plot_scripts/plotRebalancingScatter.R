#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(cowplot, warn.conflicts = FALSE)
library(sysfonts)
library(showtext)
library(showtextdb)
showtext_auto()

args <- commandArgs(trailingOnly=TRUE)
file <- args[1]
plot_filename <- args[2]
limit <-200000
data<-read.csv(file)
data$SuccRatio <- data$SuccRatio * 30 * 106/100
data$RebalancingNum <- data$RebalancingNum * 106 + (30 * 106 - data$SuccRatio) 
data$offload <- data$RebalancingNum / data$SuccRatio
data$RebalancingAmt <- data$RebalancingAmt * 106 
offload_val <- aggregate(data$offload, by = list(data$Scheme), min)
head(offload_val)


label_list <- c("PS" = "Diffindo",
                 "SP" = "Shortest Path",
                 "LR" = "Landmark Routing",
                 "WF" = "Waterfilling", 
                 "DCTCP_qdelay" = "Spider",
                 "LND" = "LND",
                 "Circ" = "Circulation")

break_list <- c("PS", "SP", "LR", "WF", "LND","Circ", "DCTCP_qdelay")

color_list <- c(
                "SP" = "#e51a1c",
                "PS" = "#377eb8",
                "LR" = "#4daf4a",
                "WF" ="#984ea3",
                "LND" = "#fb9a99",
                "DCTCP_qdelay" = "#0c2c84",
                "Circ" = "#000000")

shape_list <- c(
                "PS" = 18,
                "LND" = 8,
                "LR" = 7,
                "SP" = 4,
                "WF" = 15,
                "DCTCP_qdelay" = 19,
                "Circ" = 17)

# add a line plot
rebalancing_num_plot <- ggplot(data,
                          aes_string(x="SuccRatio",y="RebalancingNum", colour="Scheme", shape="Scheme")) +
        geom_point(size=3) +
        #geom_errorbar(aes_string(ymin="SuccRatioMin", ymax="SuccRatioMax"),width=0.2) +
        #scale_x_continuous(expand=c(0,0), trans="log10") +
        #coord_cartesian(xlim=c(1,4000),ylim=c(0,2)) +
        labs(y="Number of Rebalancing Txs (/s)", x="Successful Transactions (/s)") +
        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL, nrow=1)) +

        scale_shape_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=shape_list,
                    guide=guide_legend(title=NULL, nrow =1)) +

        theme_minimal(base_size=10) +
        theme(axis.text.x=element_text(size=rel(1.3)), axis.text.y=element_text(size=rel(1.3))) + 
        theme(legend.text=element_text(size=rel(1)), legend.key.size=unit(15,"points"), legend.position="none",
              legend.box.margin=margin(-10,-10,-10,-10),
              legend.margin=margin(c(0,0,0,0)))


ggsave(paste(plot_filename,"RebalancingNum.pdf", sep=""), width=5.7, height=5)


# add a line plot
rebalancing_amt_plot <- ggplot(data, aes_string(x="SuccRatio",y="RebalancingAmt", 
                                colour="Scheme", shape="Scheme")) +
        geom_point(size=3) +
        #geom_errorbar(aes_string(ymin="SuccRatioMin", ymax="SuccRatioMax"),width=0.2) +
        #scale_x_continuous(expand=c(0,0), trans="log10") +
        #coord_cartesian(xlim=c(1,limit)) +
        labs(y="Amount Rebalanced (€/s)", x="Txns/s") +
        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL, nrow=1)) +

        scale_shape_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=shape_list,
                    guide=guide_legend(title=NULL, nrow =1)) +

        theme_minimal(base_size=10) + 
        theme(axis.text.x=element_text(size=rel(1.3)), axis.text.y=element_text(size=rel(1.3))) + 
        theme(legend.text=element_text(size=rel(1)), legend.key.size=unit(15,"points"), legend.position="none",
              legend.box.margin=margin(-10,-10,-10,-10),
              legend.margin=margin(c(0,0,0,0)))

ggsave(paste(plot_filename, "RebalancingAmt.pdf", sep=""), width=5.7, height=5)
legend <- get_legend(rebalancing_amt_plot + theme(legend.position="top"))


prow <- plot_grid(rebalancing_num_plot + theme(legend.position="none"),
                  rebalancing_amt_plot + theme(legend.position="none"), 
                  ncol = 2, align = "v", axis = "l")

# this tells it what order to put it in
# so basically tells it put legend first then plots with th legend height 20% of the 
# plot
p <- plot_grid(legend, prow, rel_heights=c(.2,1), ncol =1)

ggsave(paste(plot_filename,"RebalancingScatterSummary.pdf", sep=""), width=12.2, height=5)
